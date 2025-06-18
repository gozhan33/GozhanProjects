using System.Threading.Tasks;
using Google.Cloud.Storage.V1;
using System.Text;

namespace KeyValueStore.Providers
{
    public class GcsProvider : IStorageProvider
    {
        private readonly StorageClient _client;
        private readonly string _bucketName;

        public GcsProvider(StorageClient client, string bucketName)
        {
            _client = client;
            _bucketName = bucketName;
        }

        private string GetObjectName(string partition, string key) => $"{partition}/{key}.txt";

        public async Task SetAsync(string partition, string key, string value)
        {
            var bytes = Encoding.UTF8.GetBytes(value);
            using var ms = new System.IO.MemoryStream(bytes);
            await _client.UploadObjectAsync(_bucketName, GetObjectName(partition, key), null, ms);
        }

        public async Task<string?> GetAsync(string partition, string key)
        {
            try
            {
                using var ms = new System.IO.MemoryStream();
                await _client.DownloadObjectAsync(_bucketName, GetObjectName(partition, key), ms);
                return Encoding.UTF8.GetString(ms.ToArray());
            }
            catch (Google.GoogleApiException ex) when (ex.Error?.Code == 404)
            {
                return null;
            }
        }

        public async Task DeleteAsync(string partition, string key)
        {
            await _client.DeleteObjectAsync(_bucketName, GetObjectName(partition, key));
        }
    }
}
