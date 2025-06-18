using System.Diagnostics;
using System.Threading.Tasks;

namespace KeyValueStore
{
    public static class KeyValueStoreBasicTests
    {
        private class InMemoryKeyValueStore : IKeyValueStore
        {
            private readonly Dictionary<string, string> _store = new();
            public Task SetAsync(string key, string value)
            {
                _store[key] = value;
                return Task.CompletedTask;
            }
            public Task<string> GetAsync(string key)
            {
                _store.TryGetValue(key, out var value);
                return Task.FromResult(value ?? string.Empty);
            }
            public Task DeleteAsync(string key)
            {
                _store.Remove(key);
                return Task.CompletedTask;
            }
        }

        public static async Task RunBasicTest()
        {
            var store = new InMemoryKeyValueStore();
            string key = "k1";
            string value = "v1";

            await store.SetAsync(key, value);
            var result = await store.GetAsync(key);
            Debug.Assert(result == value, "Set/Get failed");

            await store.DeleteAsync(key);
            var deleted = await store.GetAsync(key);
            Debug.Assert(deleted == null, "Delete failed");

            var fsStore = new FileSystemKeyValueStore(".\\TestStore");
            await fsStore.SetAsync(key, value);
            result = await fsStore.GetAsync(key);
            Debug.Assert(result == value, "Set/Get failed");

            await fsStore.DeleteAsync(key);
            deleted = await fsStore.GetAsync(key);
            Debug.Assert(deleted == null, "Delete failed");
        }
    }
}
