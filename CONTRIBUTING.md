# Contributing to fastwad

Thank you for your interest in contributing to **fastwad**! We welcome contributions, bug reports, and feature suggestions from the community.

---

## 🛠️ Development Setup

`fastwad` is written in standard modern C++17 and uses CMake for its build system. It has zero external package dependencies (all required STB headers are vendored in `third_party/stb`).

### Prerequisites

- **C++ Compiler**:
  - Windows: MSVC 2019+ (Visual Studio 2019 or later)
  - Linux: GCC 9+ or Clang 10+
  - macOS: Apple Clang 12+
- **CMake**: 3.16 or later

### Building Locally

#### Windows (PowerShell / Command Prompt)

```powershell
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFASTWAD_BUILD_TESTS=ON

# Build
cmake --build build --config Release

# Run E2E Test Suite
ctest --test-dir build -C Release --output-on-failure
```

#### Linux / macOS (Bash)

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFASTWAD_BUILD_TESTS=ON

# Build
cmake --build build --config Release -j$(nproc)

# Run E2E Test Suite
ctest --test-dir build -C Release --output-on-failure
```

---

## 🧪 Testing Guidelines

Before opening a pull request, ensure all tests pass locally:

```bash
ctest --test-dir build -C Release --output-on-failure --verbose
```

If adding new features or bug fixes:
1. Add corresponding automated test cases to `src/tests.cpp`.
2. Ensure your changes preserve bit-for-bit determinism and cross-platform compatibility (Windows MSVC, Linux GCC, Linux Clang).
3. If modifying public headers or CLI arguments, update `README.md` and `docs/index.html` accordingly.

---

## 🌿 Git Workflow & Pull Requests

1. **Fork the repository** on GitHub.
2. **Create a topic branch** from `main`:
   ```bash
   git checkout -b feat/your-feature-name
   ```
3. **Commit your changes**:
   - Write clear, concise commit messages in English.
   - Use conventional commit prefixes (e.g. `feat:`, `fix:`, `docs:`, `test:`).
4. **Push your branch and open a Pull Request**:
   - Fill out the PR template describing your changes and motivation.
   - Ensure all automated CI matrix checks (Windows MSVC, Linux GCC, Linux Clang) are green.

---

## 📜 Code of Conduct

All contributors and maintainers are expected to adhere to our [Code of Conduct](CODE_OF_CONDUCT.md).
