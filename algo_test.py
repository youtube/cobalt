#!/usr/bin/env python3
"""
Test script for the backwards DAG path-finding algorithm.
Uses pre-extracted commit data to avoid re-extraction every time.
"""

import json
import sys
import os

def load_extracted_data(filename):
    """Load the pre-extracted commit graph data."""
    print(f"📁 Loading extracted data from {filename}...")
    
    try:
        with open(filename, 'r') as f:
            data = json.load(f)
    except Exception as e:
        print(f"❌ Error loading file: {e}")
        return None
    
    # Check if we have all_commits (full graph data)
    if "all_commits" not in data:
        print(f"❌ File doesn't contain 'all_commits' graph data")
        return None
    
    print(f"✅ Loaded {len(data['all_commits'])} commits from extraction")
    return data

def convert_to_graph_format(extracted_data):
    """Convert the extracted spine data to the graph format expected by DAG algorithm."""
    
    # Extract the all_commits section and convert to the node format
    nodes = []
    for commit in extracted_data["all_commits"]:
        # Convert from spine format to graph node format
        node = {
            "hexsha": commit["hexsha"],
            "short_sha": commit["hexsha"][:8],
            "summary": commit["summary"],
            "message": commit.get("summary", ""),  # Use summary as message for now
            "author": {
                "name": commit["author"],
                "email": f"{commit['author'].lower().replace(' ', '.')}@unknown.com"  # Dummy email
            },
            "committer": {
                "name": commit["author"],
                "email": f"{commit['author'].lower().replace(' ', '.')}@unknown.com"  # Dummy email
            },
            "authored_date": commit["date"],
            "committed_date": commit["date"],
            "parents": commit["parents"],
            "parent_count": len(commit["parents"]),
            "is_merge": commit["is_merge"],
            "pr_number": None,
            "has_pr": False,
            "stats": {
                "files_changed": 1,
                "insertions": 10,
                "deletions": 5,
                "total_changes": 15
            }
        }
        
        # Extract PR number if present in summary
        import re
        pr_match = re.search(r'\(#(\d+)\)', commit["summary"])
        if pr_match:
            node["pr_number"] = pr_match.group(1)
            node["has_pr"] = True
        
        nodes.append(node)
    
    # Find the actual end commit SHA (last commit in the spine)
    end_commit_sha = extracted_data["spine_path"][-1]["hexsha"] if extracted_data["spine_path"] else nodes[-1]["hexsha"]
    
    # Create graph data structure
    graph_data = {
        "extraction_info": {
            "start_commit": extracted_data["spine_info"]["start_commit"],
            "end_commit": end_commit_sha,  # Use actual SHA instead of "main"
            "source_branch": extracted_data["spine_info"]["source_branch"],
            "total_commits": len(nodes),
            "extraction_method": "converted_from_spine_data",
            "timestamp": extracted_data["spine_info"]["timestamp"]
        },
        "nodes": nodes
    }
    
    return graph_data

def run_dag_algorithm(graph_data):
    """Run the backwards DAG path-finding algorithm."""
    print(f"\n🧭 Running backwards DAG algorithm...")
    
    # Import the DAG algorithm from main.py
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    from main import find_optimal_path_through_dag
    
    result = find_optimal_path_through_dag(graph_data)
    
    if result:
        print(f"✅ DAG algorithm completed successfully!")
        print(f"📊 Found path with {result['statistics']['total_commits']} commits")
        print(f"📊 PR coverage: {result['statistics']['pr_percentage']:.1f}%")
        
        # Show problematic commits (Draft PRs, merge branches without PRs, etc.)
        print(f"\n🔍 Analyzing spine quality...")
        
        problematic_commits = []
        for commit in result["path"]:
            summary_lower = commit["summary"].lower()
            
            # Check for problematic patterns
            if "draft pr" in summary_lower:
                problematic_commits.append((commit, "Draft PR - should not be in spine"))
            elif "merge branch" in summary_lower and not commit["has_pr"]:
                problematic_commits.append((commit, "Merge branch without PR - workflow artifact"))
            elif summary_lower.startswith("wip") or "work in progress" in summary_lower:
                problematic_commits.append((commit, "WIP commit - should not be in spine"))
        
        if problematic_commits:
            print(f"⚠️  Found {len(problematic_commits)} problematic commits in spine:")
            for commit, reason in problematic_commits:
                print(f"   🚫 {commit['short_sha']} - {commit['summary'][:60]}")
                print(f"      Reason: {reason}")
        else:
            print(f"✅ No obviously problematic commits found in spine")
        
        return result
    else:
        print(f"❌ DAG algorithm failed")
        return None

def save_results(result, graph_data, output_file):
    """Save the DAG algorithm results."""
    if not result:
        return
    
    output_data = {
        "spine_info": {
            **graph_data["extraction_info"],
            "method": "backwards_dag_pathfinding",
            "spine_commits": result["statistics"]["total_commits"]
        },
        "spine_path": result["path"],
        "statistics": result["statistics"]
    }
    
    print(f"\n💾 Saving results to {output_file}...")
    with open(output_file, 'w') as f:
        json.dump(output_data, f, indent=2)
    
    print(f"✅ Results saved!")

def main():
    """Main test function."""
    if len(sys.argv) < 2:
        print("Usage: python algo_test.py <extracted_data.json> [output_file.json]")
        print("Example: python algo_test.py full_spine_final.json dag_results.json")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else "dag_test_results.json"
    
    # Load extracted data
    extracted_data = load_extracted_data(input_file)
    if not extracted_data:
        sys.exit(1)
    
    # Convert to graph format
    print(f"\n🔄 Converting to graph format...")
    graph_data = convert_to_graph_format(extracted_data)
    print(f"✅ Converted to graph with {len(graph_data['nodes'])} nodes")
    
    # Run DAG algorithm
    result = run_dag_algorithm(graph_data)
    
    # Save results
    save_results(result, graph_data, output_file)
    
    # Show summary
    if result:
        print(f"\n📋 Top 10 commits in DAG spine:")
        for i, commit in enumerate(result["path"][:10]):
            pr_indicator = f"#{commit['pr_number']}" if commit["has_pr"] else "no-PR"
            print(f"   {i+1:2d}. {commit['short_sha']} ({pr_indicator}) - {commit['summary'][:60]}")
        
        if len(result["path"]) > 10:
            print(f"   ... and {len(result['path']) - 10} more commits")

if __name__ == "__main__":
    main()