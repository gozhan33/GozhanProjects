# KeyValueStore

A simple, modular C# Key-Value Store supporting multiple backends:
- Windows File System
- Azure Blob Storage
- Amazon S3
- Google Cloud Storage

## Features
- Pluggable storage backends
- Partitioned keys for scalability

## Getting Started
1. Ensure you have [.NET 8 SDK](https://dotnet.microsoft.com/download) installed.
2. Restore dependencies: `dotnet restore`
3. Build the project: `dotnet build`

## Usage Example

```csharp
using KeyValueStore;
using KeyValueStore.Providers;

// Example: Using the FileSystemProvider
var provider = new FileSystemProvider(@"C:\\data\\kvstore");
var store = new PartitionedKeyValueStore(provider);

await store.SetAsync("partition1", "key1", "value1");
string? value = await store.GetAsync("partition1", "key1");
await store.DeleteAsync("partition1", "key1");
```

> For Azure, S3, or GCS, use the corresponding provider and pass the required credentials/configuration.

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## License
MIT
