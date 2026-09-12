# Explain Action

1. Read `context/current-feature.md` to understand what was implemented.
2. Run `git diff main --name-only` (or base branch) to get the list of modified and new files.
3. For the entire feature:
   - **Start with a Relatable Real-World Analogy**: Use intuitive real-world metaphors (e.g., everyday gadgets, cars, cooking, plumbing, weather instincts, factory lines) to explain the overall concept simply (like explaining to a bright 15-year-old).
4. For each file created or modified:
   - Show the file path (`.c`, `.h`, `.ld`, Makefile/CMakeLists.txt, `.py`).
   - Give a clear 1-2 sentence plain-English explanation of what it does and why it was changed, avoiding dense jargon.
   - Use concrete real-world comparisons to explain functions, structs, registers, or mathematical formulas.
5. **How It All Connects (The Step-by-Step Story)**:
   - Provide an intuitive diagram or journey tracking raw physical measurements $\rightarrow$ smart edge math $\rightarrow$ final field action (e.g., siren/LoRa/valve).
6. **Why This Matters in Practice**:
   - Highlight the tangible real-world benefit (e.g., saving battery, preventing false alarms during solar heating, protecting tea crops from rain).

## Output Format

### 💡 The Big Picture (Real-World Analogy)
[Vivid, relatable metaphor explaining the core problem and how this feature solves it in plain English]

### 📁 Files Changed
- **`path/to/driver_bme280.c`** *(New/Modified)*
  - *What it is*: [Simple plain-English description]
  - *Real-world analogy*: [Everyday comparison, e.g. "Like a thermometer with a power switch..."]
  - *Key functions*: [Plain explanation of functions]

### 🔗 How It All Connects
[Step-by-step data journey diagram and narrative from physical sensation to decision]

### 🌾 Why This Matters in the Field
[Practical estate-level benefit, e.g. preventing wasted fertilizer sprays or saving battery life]