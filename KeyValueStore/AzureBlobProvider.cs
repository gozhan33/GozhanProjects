using System.Threading.Tasks;
using Azure.Storage.Blobs;
using System.Text;

namespace KeyValueStore.Providers
{
    public class AzureBlobProvider : IStorageProvider
    {
        private readonly BlobServiceClient _serviceClient;
        private readonly string _containerName;

        public AzureBlobProvider(string connectionString, string containerName)
        {
            _serviceClient = new BlobServiceClient(connectionString);
            _containerName = containerName;
        }

        private BlobClient GetBlobClient(string partition, string key)
        {
            var container = _serviceClient.GetBlobContainerClient(_containerName);
            container.CreateIfNotExists();
            var blobName = $"{partition}/{key}.txt";
            return container.GetBlobClient(blobName);
        }

        public async Task SetAsync(string partition, string key, string value)
        {
            var blob = GetBlobClient(partition, key);
            var bytes = Encoding.UTF8.GetBytes(value);
            using var ms = new System.IO.MemoryStream(bytes);
            await blob.UploadAsync(ms, overwrite: true);
        }

        public async Task<string?> GetAsync(string partition, string key)
        {
            var blob = GetBlobClient(partition, key);
            if (!await blob.ExistsAsync()) return null;
            var download = await blob.DownloadContentAsync();
            return download.Value.Content.ToString();
        }

        public async Task DeleteAsync(string partition, string key)
        {
            var blob = GetBlobClient(partition, key);
            await blob.DeleteIfExistsAsync();
        }
    }
}
