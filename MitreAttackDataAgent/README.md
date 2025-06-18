# MitreAttackDataAgent

This solution provides an OpenAI-powered agent to access and query MITRE ATT&CK data.

## Projects
- **KeyValueStore**: Abstraction for storing data in Azure Blob Storage, Windows File System, or AWS S3.
- **MitreAttackData**: Fetches, parses, and queries MITRE ATT&CK data.
- **OpenAIAgent**: Handles prompt processing and integrates with OpenAI.
- **Tests**: Unit and integration tests for all components.

## Getting Started
1. Build the solution:
   ```powershell
   dotnet build
   ```
2. Run tests:
   ```powershell
   dotnet test
   ```
3. Extend each project as needed for your use case.

## Requirements
- .NET 6.0 or later
- Internet access for fetching MITRE ATT&CK data and using OpenAI

## Next Steps
- Implement MITRE ATT&CK data integration in `MitreAttackData`.
- Implement OpenAI integration in `OpenAIAgent`.
- Implement storage providers in `KeyValueStore`.
