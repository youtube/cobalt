//! Parsing for CFF FDSelect tables.

use types::GlyphId;

use super::FdSelect;

impl<'a> FdSelect<'a> {
    /// Returns the associated font DICT index for the given glyph identifier.
    pub fn font_index(&self, glyph_id: GlyphId) -> Option<u16> {
        match self {
            // See <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-11-fdselect-format-0>
            Self::Format0(fds) => fds
                .fds()
                .get(glyph_id.to_u16() as usize)
                .map(|fd| *fd as u16),
            // See <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-12-fdselect-format-3>
            Self::Format3(fds) => {
                let ranges = fds.ranges();
                let gid = glyph_id.to_u16();
                let ix = match ranges.binary_search_by(|range| range.first().cmp(&gid)) {
                    Ok(ix) => ix,
                    Err(ix) => ix.saturating_sub(1),
                };
                Some(ranges.get(ix)?.fd() as u16)
            }
            // See <https://learn.microsoft.com/en-us/typography/opentype/spec/cff2#table-14-fdselect-format-4>
            Self::Format4(fds) => {
                let ranges = fds.ranges();
                let gid = glyph_id.to_u16() as u32;
                let ix = match ranges.binary_search_by(|range| range.first().cmp(&gid)) {
                    Ok(ix) => ix,
                    Err(ix) => ix.saturating_sub(1),
                };
                Some(ranges.get(ix)?.fd())
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{FdSelect, GlyphId};
    use crate::test_helpers::BeBuffer;
    use crate::FontRead;
    use std::ops::Range;

    #[test]
    fn select_font_index() {
        let map = &[
            (0..10, 0),
            (10..32, 4),
            (32..34, 1),
            (34..128, 12),
            (128..1024, 2),
        ];
        let fd_select_datas = make_fd_selects(map);
        for data in fd_select_datas {
            let fd_select = FdSelect::read(data.font_data()).unwrap();
            for (range, font_index) in map {
                for gid in range.clone() {
                    assert_eq!(
                        fd_select.font_index(GlyphId::new(gid)).unwrap() as u8,
                        *font_index
                    )
                }
            }
        }
    }

    /// Builds FDSelect structures in all three formats for the given
    /// Range<GID> -> font index mapping.
    fn make_fd_selects(map: &[(Range<u16>, u8)]) -> [BeBuffer; 3] {
        let glyph_count = map.last().unwrap().0.end;
        let format0 = {
            let mut buf = BeBuffer::new();
            buf = buf.push(0u8);
            let mut fds = vec![0u8; glyph_count as usize];
            for (range, font_index) in map {
                for gid in range.clone() {
                    fds[gid as usize] = *font_index;
                }
            }
            buf = buf.extend(fds);
            buf
        };
        let format3 = {
            let mut buf = BeBuffer::new();
            buf = buf.push(3u8);
            buf = buf.push(map.len() as u16);
            for (range, font_index) in map {
                buf = buf.push(range.start);
                buf = buf.push(*font_index);
            }
            buf = buf.push(glyph_count);
            buf
        };
        let format4 = {
            let mut buf = BeBuffer::new();
            buf = buf.push(4u8);
            buf = buf.push(map.len() as u32);
            for (range, font_index) in map {
                buf = buf.push(range.start as u32);
                buf = buf.push(*font_index as u16);
            }
            buf = buf.push(glyph_count as u32);
            buf
        };
        [format0, format3, format4]
    }
}
