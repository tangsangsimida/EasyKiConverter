# Architecture Decision Records

This directory contains Architecture Decision Records (ADRs) for the EasyKiConverter project.

## What is an ADR?

ADR (Architecture Decision Records) is a lightweight document for recording "why we made a certain important technical decision". It helps us:

- Understand historical project decisions
- Provide context for new decisions
- Facilitate team communication
- Avoid repeatedly discussing the same issues

## ADR Format

Each ADR should contain the following sections:

1. **Status**: Proposed, Accepted, Deprecated, Superseded
2. **Context**: What is the problem? Why do we need to make this decision?
3. **Decision**: What have we decided to do?
4. **Consequences**: What are the results of this decision? Including positive and negative consequences

## ADR Naming Convention

ADR files should use the following naming format:

```
<number>-<short-description>.md
```

For example:
- `001-mvvm-architecture.md`
- `002-qt-quick-ui-framework.md`
- `003-two-stage-export-strategy.md`

## ADR Process

### Creating a New ADR

1. Create a new ADR file with status set to "Proposed"
2. Discuss the decision within the team
3. Modify the ADR based on feedback
4. If the decision is accepted, update status to "Accepted"
5. If the decision is rejected, update status to "Rejected"

### Updating an ADR

If the decision changes:

1. Update the ADR content
2. Update status (if needed)
3. Create a new ADR to record the new decision
4. Reference the new ADR in the old ADR

## Existing ADRs

- [ADR 001: Choose MVVM Architecture](001-mvvm-architecture_en.md)
- [ADR 001: 选择 MVVM 架构](001-mvvm-architecture.md) (中文)

## References

- [Architecture Decision Records](https://adr.github.io/)
- [Recording Architecture Decisions](https://www.thoughtworks.com/radar/techniques/recording-architecture-decisions)