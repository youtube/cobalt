-- Do not remove: This is the query to check the unknown Cobalt allocations.
-- It walks up the call stack to find the deepest and entry Cobalt frames for
-- each top callsite, which helps us identify the likely AudioTimeStretcher and JNI entry points.
WITH RECURSIVE cobalt_walker(id, parent_id, frame_id, depth, cobalt_leaf, cobalt_entry) AS (
  -- Base: Start with top callsites
  SELECT
    c.id, c.parent_id, c.frame_id, 0,
    -- If the leaf itself is Cobalt, mark it
    CASE WHEN m.name LIKE '%libchrobalt.so%' THEN printf('0x%x', f.rel_pc) ELSE NULL END,
    CASE WHEN m.name LIKE '%libchrobalt.so%' THEN printf('0x%x', f.rel_pc) ELSE NULL END
  FROM stack_profile_callsite c
  JOIN stack_profile_frame f ON c.frame_id = f.id
  JOIN stack_profile_mapping m ON f.mapping = m.id
  WHERE c.id IN (SELECT callsite_id FROM heap_profile_allocation GROUP BY 1 ORDER BY SUM(size) DESC LIMIT 20)

  UNION ALL

  -- Recursive: Walk up to find the entry point
  SELECT
    cw.id, c.parent_id, c.frame_id, cw.depth + 1,
    cw.cobalt_leaf,
    -- Update entry point as we find higher Cobalt frames
    CASE WHEN m.name LIKE '%libchrobalt.so%' THEN printf('0x%x', f.rel_pc) ELSE cw.cobalt_entry END
  FROM cobalt_walker cw
  JOIN stack_profile_callsite c ON cw.parent_id = c.id
  JOIN stack_profile_frame f ON c.frame_id = f.id
  JOIN stack_profile_mapping m ON f.mapping = m.id
  WHERE cw.parent_id IS NOT NULL AND cw.depth < 50
)
SELECT
  cw.id AS callsite_id,
  SUM(a.size) / 1024 / 1024 AS size_mb,
  -- The deepest Cobalt address (likely AudioTimeStretcher 0x35cd371)
  MAX(cw.cobalt_leaf) AS deep_cobalt_address,
  -- The highest Cobalt address (likely JNI 0x35b0f3d)
  MIN(cw.cobalt_entry) AS entry_cobalt_address
FROM cobalt_walker cw
JOIN heap_profile_allocation a ON cw.id = a.callsite_id
GROUP BY 1
ORDER BY 2 DESC;
