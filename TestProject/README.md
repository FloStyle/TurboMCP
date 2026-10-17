# Integration test project

TurboMCPTest.uproject discovers the sibling Plugins directory through AdditionalPluginDirectories. Fixtures are versioned; Binaries, Intermediate and Saved are generated and ignored. Engine source and binaries stay in the separate pinned engine checkout.

Use ../Tools/build.ps1 and ../Tools/run-suites.ps1 from the repository root. Tests may modify fixtures; restore only changes owned by the test run. No engine-wide plugin mount is needed for this project.
