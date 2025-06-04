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
        }
    }
}
