using System.Threading.Tasks;
using KeyValueStore.Providers;

namespace KeyValueStore
{
    public interface IKeyValueStore
    {
        Task SetAsync(string partition, string key, string value);
        Task<string?> GetAsync(string partition, string key);
        Task DeleteAsync(string partition, string key);
    }

    public class PartitionedKeyValueStore : IKeyValueStore
    {
        private readonly IStorageProvider _provider;
        public PartitionedKeyValueStore(IStorageProvider provider)
        {
            _provider = provider;
        }

        public Task SetAsync(string partition, string key, string value)
            => _provider.SetAsync(partition, key, value);

        public Task<string?> GetAsync(string partition, string key)
            => _provider.GetAsync(partition, key);

        public Task DeleteAsync(string partition, string key)
            => _provider.DeleteAsync(partition, key);
    }
}
