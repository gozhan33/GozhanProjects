using System.Threading.Tasks;

namespace KeyValueStore.Providers
{
    public interface IStorageProvider
    {
        Task SetAsync(string partition, string key, string value);
        Task<string?> GetAsync(string partition, string key);
        Task DeleteAsync(string partition, string key);
    }
}
