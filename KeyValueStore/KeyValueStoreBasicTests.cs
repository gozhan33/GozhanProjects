using System.Threading.Tasks;
using System.Diagnostics;
using KeyValueStore;
using KeyValueStore.Providers;

namespace KeyValueStore
{
    public static class KeyValueStoreBasicTests
    {
        private class InMemoryProvider : IStorageProvider
        {
            private readonly Dictionary<string, string> _store = new();
            public Task SetAsync(string partition, string key, string value)
            {
                _store[$"{partition}:{key}"] = value;
                return Task.CompletedTask;
            }
            public Task<string?> GetAsync(string partition, string key)
            {
                _store.TryGetValue($"{partition}:{key}", out var value);
                return Task.FromResult(value);
            }
            public Task DeleteAsync(string partition, string key)
            {
                _store.Remove($"{partition}:{key}");
                return Task.CompletedTask;
            }
        }

        public static async Task RunBasicTest()
        {
            var provider = new InMemoryProvider();
            var store = new PartitionedKeyValueStore(provider);
            string partition = "p1";
            string key = "k1";
            string value = "v1";

            await store.SetAsync(partition, key, value);
            var result = await store.GetAsync(partition, key);
            Debug.Assert(result == value, "Set/Get failed");

            await store.DeleteAsync(partition, key);
            var deleted = await store.GetAsync(partition, key);
            Debug.Assert(deleted == null, "Delete failed");

            var fsProvider = new FileSystemProvider(".\\TestStore");
            var fsStore = new PartitionedKeyValueStore(fsProvider);

            await fsStore.SetAsync(partition, key, value);
            result = await fsStore.GetAsync(partition, key);
            Debug.Assert(result == value, "Set/Get failed");

            await fsStore.DeleteAsync(partition, key);
            deleted = await fsStore.GetAsync(partition, key);
            Debug.Assert(deleted == null, "Delete failed");

            // NOTE: Set your Azure Blob Storage API key here or via environment variable
            var absApiKey = Environment.GetEnvironmentVariable("AZURE_BLOB_STORAGE_API_KEY") ?? "AZURE_BLOB_STORAGE_API_KEY";
            if (absApiKey == "AZURE_BLOB_STORAGE_API_KEY")
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine("WARNING: Azure Blob Storage API key is not set. Please set the AZURE_BLOB_STORAGE_API_KEY environment variable.");
                Console.ResetColor();
                // Throw an exception to prevent accidental use in production
                throw new InvalidOperationException("Azure Blob Storage API key is not set. Set the AZURE_BLOB_STORAGE_API_KEY environment variable.");
            }
            else
            {
                Console.WriteLine("Using OpenAI API Key: " + absApiKey.Substring(0, 10) + "****");
            }
            var absProvider = new AzureBlobProvider("DefaultEndpointsProtocol=https;AccountName=gozhanstore;AccountKey=" + absApiKey, "gozhantestcontainer");
            var absStore = new PartitionedKeyValueStore(absProvider);

            await absStore.SetAsync(partition, key, value);
            result = await absStore.GetAsync(partition, key);
            Debug.Assert(result == value, "Set/Get failed");

            await absStore.DeleteAsync(partition, key);
            deleted = await absStore.GetAsync(partition, key);
            Debug.Assert(deleted == null, "Delete failed");
        }
    }
}
