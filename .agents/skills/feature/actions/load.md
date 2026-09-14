# Load Action

1. Check `$ARGUMENTS` (after "load"):
   - If it looks like a filename (single word, no spaces): Look for `context/features/{name}.md` OR `context/fixes/{name}.md`
   - If it's multiple words: Use as inline feature description, generate goals
   - If empty: Error - "load" requires a spec filename or feature description

2. Update `context/current-feature.md`:
   - Update H1 heading to include feature name (e.g., `# Current Feature: BME280 Sensor Driver`)
   - Write goals as bullet points under `## Goals`
   - Discover and populate dependencies in `## Notes`:
     - If local knowledge graph (`graphify-out/graph.json`) is available, run `graphify query "<feature/module name>"` (or check shortest paths) to discover dependent headers, caller functions, and related structs.
     - Record hardware constraints, register maps, memory limits, and discovered module dependencies under `## Notes`.
   - Set Status to `Not Started`

3. Confirm spec loaded and display the feature summary.