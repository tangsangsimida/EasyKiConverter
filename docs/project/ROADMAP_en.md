# Project Roadmap

This document shows the future direction of the project and the main features planned for the next few versions.

## Current Status

- **Current Version**: 3.0.4
- **Development Status**: Refactoring complete, entering optimization phase
- **Completion**: ~95% (core features implemented, architecture refactoring complete)

## Version Planning

### v3.0.0 - Core Feature Completion (Current Version)

**Goal**: Complete core features and improve user experience

**Core Architecture**:
- [x] Multi-stage pipeline parallelism implementation
- [x] Thread-safe bounded queues
- [x] Real-time progress feedback

**Export Functionality Enhancements**:
- [ ] Quick access to export destination folder after completion
- [x] Network request retry mechanism (implemented in v3.0.2)
- [ ] Weak network resilience improvements (timeout retry, unified backoff strategy, NetworkWorker timeout protection)
- [ ] 3D model path export options (relative/absolute)
  - [ ] Relative path: Use KiCad project path + 3D model path
  - [ ] Absolute path: Use export path + 3D model path
- [ ] Export progress bar design
- [ ] Export statistics design
- [ ] Export preview functionality (footprint, symbol, 3D model, manual PDF)

**Export Options Optimization**:
- [x] Overwrite and append export options
- [x] Export mode UI design optimization (clearer mode selection)
- [ ] Library description export option (KiCad library description parameter)
- [ ] Footprint attribute enhancements (description, dual 3D model addresses, etc.)

**Data Parsing Improvements**:
- [~] Footprint symbol parser and exporter refinement
  - [x] Footprint symbol line width parsing
  - [ ] Other attribute parsing optimizations
- [ ] BOM table parsing functionality testing
- [ ] Special component export issue fixes (e.g., C7420375 position offset)

**User Interface Improvements**:
- [ ] Component list search functionality
- [ ] Multi-language support (automatic based on system language)
- [ ] UI performance optimization and response speed improvements

**Development Tools and Processes**:
- [x] GitHub workflow configuration
  - [x] Automated code legal security review
  - [ ] Automated testing
  - [ ] Automated compilation and packaging
  - [ ] Automated version release
- [x] Contribution guidelines and project documentation improvements

### v3.1.0 - Performance Optimization and Stability Improvements (Planned)

**Goal**: Optimize performance, improve stability, complete test coverage

**Main Features**:
- [ ] Performance testing and optimization (compare with Python version)
- [ ] Memory usage optimization
- [ ] Network request testing and optimization
- [ ] UI automation testing
- [ ] Integration testing improvements
- [ ] Error handling improvements
- [ ] Logging system optimization

**Expected Release**: Q2 2026

## Feature Priorities

### High Priority

1. Performance optimization
2. Stability improvements
3. Test coverage completion
4. Error handling improvements
5. User experience optimization

### Medium Priority

1. New features addition
2. Advanced search
3. Component library management
4. Export preview functionality
5. Multi-language support

### Low Priority

1. Plugin system
2. Cloud sync
3. Mobile version
4. Enterprise edition
5. AI features

## Technical Debt

### Issues to Resolve

1. **Code Quality**
   - [ ] Increase code coverage to 80%+
   - [ ] Improve code review process
   - [ ] Unify code style

2. **Documentation**
   - [ ] Complete API documentation
   - [ ] Improve developer documentation
   - [ ] Update user manual

3. **Testing**
   - [ ] Unit test coverage
   - [ ] Integration testing improvements
   - [ ] Performance testing establishment

4. **Performance**
   - [ ] Startup speed optimization
   - [ ] Memory usage optimization
   - [ ] Network request optimization

## How to Participate

If you want to participate in project development:

1. Check the [Contributing Guide](../developer/CONTRIBUTING_en.md)
2. Choose a task you're interested in
3. Create an Issue to discuss your ideas
4. Submit a Pull Request

## Feedback and Suggestions

We welcome your feedback and suggestions:

- Submit issues on [GitHub Issues](https://github.com/tangsangsimida/EasyKiConverter_QT/issues)
- Participate in discussions on [GitHub Discussions](https://github.com/tangsangsimida/EasyKiConverter_QT/discussions)
- Contact project maintainers via email

## Changelog

This roadmap will be updated regularly. Please stay tuned for the latest updates.

## Disclaimer

This roadmap is for reference only. Actual development plans may be adjusted based on actual circumstances.
