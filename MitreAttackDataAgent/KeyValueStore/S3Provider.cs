using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using System.Text;

namespace KeyValueStore.Providers
{
    public class S3Provider : IStorageProvider
    {
        private readonly IAmazonS3 _s3Client;
        private readonly string _bucketName;

        public S3Provider(IAmazonS3 s3Client, string bucketName)
        {
            _s3Client = s3Client;
            _bucketName = bucketName;
        }

        private string GetS3Key(string partition, string key) => $"{partition}/{key}.txt";

        public async Task SetAsync(string partition, string key, string value)
        {
            var putRequest = new PutObjectRequest
            {
                BucketName = _bucketName,
                Key = GetS3Key(partition, key),
                ContentBody = value
            };
            await _s3Client.PutObjectAsync(putRequest);
        }

        public async Task<string?> GetAsync(string partition, string key)
        {
            var getRequest = new GetObjectRequest
            {
                BucketName = _bucketName,
                Key = GetS3Key(partition, key)
            };
            try
            {
                using var response = await _s3Client.GetObjectAsync(getRequest);
                using var reader = new System.IO.StreamReader(response.ResponseStream, Encoding.UTF8);
                return await reader.ReadToEndAsync();
            }
            catch (AmazonS3Exception ex) when (ex.StatusCode == System.Net.HttpStatusCode.NotFound)
            {
                return null;
            }
        }

        public async Task DeleteAsync(string partition, string key)
        {
            var deleteRequest = new DeleteObjectRequest
            {
                BucketName = _bucketName,
                Key = GetS3Key(partition, key)
            };
            await _s3Client.DeleteObjectAsync(deleteRequest);
        }
    }
}
