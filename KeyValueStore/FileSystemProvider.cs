using System;
using System.IO;
using System.Threading.Tasks;

namespace KeyValueStore.Providers
{
    public class FileSystemProvider : IStorageProvider
    {
        private readonly string _basePath;

        public FileSystemProvider(string basePath)
        {
            _basePath = basePath;
            Directory.CreateDirectory(_basePath);
        }

        private string GetFilePath(string partition, string key)
        {
            var partitionDir = Path.Combine(_basePath, partition);
            Directory.CreateDirectory(partitionDir);
            return Path.Combine(partitionDir, key + ".txt");
        }

        public async Task SetAsync(string partition, string key, string value)
        {
            var path = GetFilePath(partition, key);
            await File.WriteAllTextAsync(path, value);
        }

        public async Task<string?> GetAsync(string partition, string key)
        {
            var path = GetFilePath(partition, key);
            if (!File.Exists(path)) return null;
            return await File.ReadAllTextAsync(path);
        }

        public Task DeleteAsync(string partition, string key)
        {
            var path = GetFilePath(partition, key);
            if (File.Exists(path)) File.Delete(path);
            return Task.CompletedTask;
        }
    }
}
