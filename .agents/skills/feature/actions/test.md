# Test Action

1. Read `context/current-feature.md` to understand the implemented firmware functionality.
2. Identify core functions, sensor data parsers, mathematical algorithms (e.g., pressure drop heuristics, dew point calculation), ring buffer operations, or protocol payload encoders/decoders added or modified.
3. Check if unit or host test suites already exist for these modules.
4. For modules with testable algorithmic, driver, or parser logic:
   - Create or update unit test files using host C testing frameworks (Unity, Ceedling, or equivalent runner) or Python simulation scripts.
   - Test nominal happy path and extreme edge conditions (e.g., barometric drops, saturation humidity 100%, 0 lux darkness, CRC checksum failures, communication timeouts).
   - Verify floating-point/fixed-point numerical stability and bounds clamping.
5. Run the test harness / build suite to verify all test cases pass without errors or memory violations.
6. Report test execution results and coverage summary.