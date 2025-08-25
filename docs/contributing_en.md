# 🤝 Contribution Guide

Welcome to contribute to the EasyKiConverter project! Here are some basic contribution guidelines:

## 📋 Contribution Process

1. **Fork the Repository**
   - Click the Fork button on GitHub to create a copy of the project.

2. **Clone the Repository**
```bash
git clone <your-fork-url>
cd EasyKiConverter
```

3. **Create a Branch**
```bash
git checkout -b feature/your-feature-name
```

4. **Build and Test**
```bash
mkdir build
cd build
cmake ..
make
ctest
```

5. **Commit Changes**
```bash
git add .
git commit -m "Describe your changes"
git push origin feature/your-feature-name
```

6. **Create a Pull Request**
   - Go to your fork page on GitHub.
   - Click the "New Pull Request" button.
   - Describe your changes and submit the PR.

## 🧰 Development Guidelines

- Follow the project's coding style guide
- Use CMake build system
- Ensure all new features have unit tests
- Run `ctest` before submitting to ensure all tests pass

## 📚 Documentation Contributions

- Update [README.md](../README.md) and [README_en.md](../README_en.md)
- Modify [Development Guide](development_guide_en.md) and [Contribution Guide](contributing_en.md)
- Ensure documentation is clear, accurate, and consistent with the code

## 🛠️ Tool Usage

- Use Visual Studio Code or CLion for development
- Configure CMake Tools extension to simplify the build process
- Use GDB or LLDB for debugging

## 📂 Directory Structure
Refer to [Project Structure](project_structure_en.md) document

## 📋 Contribution Types

- 🐛 **Bug Fixes**: Fix existing functionality issues
- ✨ **New Features**: Add new functionality
- 📚 **Documentation**: Improve documentation and instructions
- 🎨 **UI/UX**: Improve user interface and experience
- ⚡ **Performance**: Optimize performance and efficiency
- 🧪 **Testing**: Add or improve tests

## 🔍 Code Review

- All PRs require code review
- Maintainers will review your code and provide feedback
- Please respond to review comments promptly and make necessary changes
- After review approval, PR will be merged into `dev` branch

## 🚀 Release Process

- `dev` branch is used for daily development and feature integration
- Release versions are created periodically from `dev` branch to `main` branch
- All stable features will be released at appropriate times

## 💡 Contribution Guidelines

- Recommend creating an Issue for discussion before starting large feature development
- Keep commit messages clear and meaningful
- Follow project coding standards
- Ensure your code is tested before submission

## 🐛 Reporting Issues
- Use [GitHub Issues](https://github.com/tangsangsimida/EasyKiConverter/issues)
- Provide detailed error information and reproduction steps
- Include LCSC component numbers and system information

## 💡 Feature Suggestions
- Describe new feature requirements in Issues
- Explain use cases and expected effects
- Participate in community discussions and contributions