using System;
using System.IO;
using System.Threading.Tasks;

namespace KeyValueStore
{
    public interface IKeyValueStore
    {
        Task SetAsync(string key, string value);
        Task<string> GetAsync(string key);
        Task DeleteAsync(string key);
    }

    // Windows File System implementation
    public class FileSystemKeyValueStore : IKeyValueStore
    {
        private readonly string _basePath;
        public FileSystemKeyValueStore(string basePath)
        {
            _basePath = basePath;
            Directory.CreateDirectory(_basePath);
        }
        public async Task SetAsync(string key, string value)
        {
            var path = Path.Combine(_basePath, key);
            await File.WriteAllTextAsync(path, value);
        }
        public async Task<string> GetAsync(string key)
        {
            var path = Path.Combine(_basePath, key);
            return File.Exists(path) ? await File.ReadAllTextAsync(path) : null;
        }
        public Task DeleteAsync(string key)
        {
            var path = Path.Combine(_basePath, key);
            if (File.Exists(path)) File.Delete(path);
            return Task.CompletedTask;
        }
    }

    // Azure Blob Storage implementation
    public class AzureBlobKeyValueStore : IKeyValueStore
    {
        private readonly Azure.Storage.Blobs.BlobContainerClient _container;
        public AzureBlobKeyValueStore(string connectionString, string containerName)
        {
            _container = new Azure.Storage.Blobs.BlobContainerClient(connectionString, containerName);
            _container.CreateIfNotExists();
        }
        public async Task SetAsync(string key, string value)
        {
            var blob = _container.GetBlobClient(key);
            using var ms = new System.IO.MemoryStream(System.Text.Encoding.UTF8.GetBytes(value));
            await blob.UploadAsync(ms, overwrite: true);
        }
        public async Task<string> GetAsync(string key)
        {
            var blob = _container.GetBlobClient(key);
            if (!await blob.ExistsAsync()) return null;
            var download = await blob.DownloadContentAsync();
            return download.Value.Content.ToString();
        }
        public async Task DeleteAsync(string key)
        {
            var blob = _container.GetBlobClient(key);
            await blob.DeleteIfExistsAsync();
        }
    }

    // AWS S3 implementation
    public class S3KeyValueStore : IKeyValueStore
    {
        private readonly Amazon.S3.IAmazonS3 _s3;
        private readonly string _bucket;
        public S3KeyValueStore(string accessKey, string secretKey, string bucketName, string region)
        {
            _bucket = bucketName;
            _s3 = new Amazon.S3.AmazonS3Client(accessKey, secretKey, Amazon.RegionEndpoint.GetBySystemName(region));
        }
        public async Task SetAsync(string key, string value)
        {
            var putRequest = new Amazon.S3.Model.PutObjectRequest
            {
                BucketName = _bucket,
                Key = key,
                ContentBody = value
            };
            await _s3.PutObjectAsync(putRequest);
        }
        public async Task<string> GetAsync(string key)
        {
            try
            {
                var getRequest = new Amazon.S3.Model.GetObjectRequest { BucketName = _bucket, Key = key };
                using var response = await _s3.GetObjectAsync(getRequest);
                using var reader = new System.IO.StreamReader(response.ResponseStream);
                return await reader.ReadToEndAsync();
            }
            catch (Amazon.S3.AmazonS3Exception e) when (e.StatusCode == System.Net.HttpStatusCode.NotFound)
            {
                return null;
            }
        }
        public async Task DeleteAsync(string key)
        {
            await _s3.DeleteObjectAsync(_bucket, key);
        }
    }

    // Partitioned Key-Value Store implementation
    public class PartitionedKeyValueStore : IKeyValueStore
    {
        private readonly IKeyValueStore _inner;
        private readonly string _partition;
        public PartitionedKeyValueStore(IKeyValueStore inner, string partition = "default")
        {
            _inner = inner;
            _partition = partition;
        }
        private string GetPartitionedKey(string key) => $"{_partition}:{key}";
        public Task SetAsync(string key, string value) => _inner.SetAsync(GetPartitionedKey(key), value);
        public Task<string> GetAsync(string key) => _inner.GetAsync(GetPartitionedKey(key));
        public Task DeleteAsync(string key) => _inner.DeleteAsync(GetPartitionedKey(key));
    }
}
