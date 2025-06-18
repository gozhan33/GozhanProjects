using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using MitreAttackData;
using OpenAIAgent;
using KeyValueStore;
using Xunit;

namespace Tests
{
    public class MitreAttackDataProviderTests
    {
        [Fact]
        public async Task GetAllTechniquesAsync_ReturnsTechniques()
        {
            var provider = new MitreAttackDataProvider();
            var techniques = await provider.GetAllTechniquesAsync();
            Assert.NotNull(techniques);
            Assert.True(techniques.Any());
        }

        [Fact]
        public async Task SearchTechniquesAsync_FindsRelevantTechniques()
        {
            var provider = new MitreAttackDataProvider();
            var results = await provider.SearchTechniquesAsync("AI");
            Assert.NotNull(results);
            Assert.True(results.Any());
        }

        [Fact]
        public async Task SearchTechniquesAsync_ReturnsEmptyForNonsense()
        {
            var provider = new MitreAttackDataProvider();
            var results = await provider.SearchTechniquesAsync("nonsensewordthatshouldnotmatch");
            Assert.NotNull(results);
            Assert.False(results.Any());
        }
    }

    public class OpenAIAgentTests
    {
        private class FakeMitreProvider : IMitreAttackDataProvider
        {
            public Task<IEnumerable<AttackTechnique>> GetAllTechniquesAsync() =>
                Task.FromResult<IEnumerable<AttackTechnique>>(new[]
                {
                    new AttackTechnique { Id = "T1001", Name = "AI Technique", Description = "AI related technique", Tags = new List<string>{"AI"} }
                });

            public Task<IEnumerable<AttackTechnique>> SearchTechniquesAsync(string keyword) =>
                Task.FromResult<IEnumerable<AttackTechnique>>(new[]
                {
                    new AttackTechnique { Id = "T1001", Name = "AI Technique", Description = "AI related technique", Tags = new List<string>{"AI"} }
                });
        }

        private class FakeOpenAIAgent : OpenAIAgent.OpenAIAgent
        {
            public FakeOpenAIAgent(IMitreAttackDataProvider mitreProvider) : base(mitreProvider, "fake-key") { }
            public override async Task<string> GetMitreAttackResponseAsync(string prompt)
            {
                // Simulate a response
                await Task.Delay(10);
                return $"Simulated response for prompt: {prompt}";
            }
        }

        [Fact]
        public async Task GetMitreAttackResponseAsync_ReturnsResponse()
        {
            var agent = new FakeOpenAIAgent(new FakeMitreProvider());
            var response = await agent.GetMitreAttackResponseAsync("Tell me about AI attacks");
            Assert.NotNull(response);
            Assert.Contains("Simulated response", response);
        }
    }

    public class UnitTest1
    {
        [Fact]
        public void Test1()
        {

        }
    }

    public class KeyValueStoreTests
    {
        private const string TestDir = "kvstore_test";

        [Fact]
        public async Task FileSystemKeyValueStore_SetAndGet_Works()
        {
            if (System.IO.Directory.Exists(TestDir))
                System.IO.Directory.Delete(TestDir, true);
            var store = new KeyValueStore.FileSystemKeyValueStore(TestDir);
            await store.SetAsync("foo", "bar");
            var value = await store.GetAsync("foo");
            Assert.Equal("bar", value);
        }

        [Fact]
        public async Task FileSystemKeyValueStore_Delete_RemovesKey()
        {
            if (System.IO.Directory.Exists(TestDir))
                System.IO.Directory.Delete(TestDir, true);
            var store = new KeyValueStore.FileSystemKeyValueStore(TestDir);
            await store.SetAsync("foo", "bar");
            await store.DeleteAsync("foo");
            var value = await store.GetAsync("foo");
            Assert.Null(value);
        }
    }

    public class AzureBlobKeyValueStoreTests
    {
        // These tests require a real Azure Storage account and container.
        // To enable, set the following environment variables:
        // AZURE_BLOB_CONNECTION_STRING, AZURE_BLOB_CONTAINER
        private static string ConnectionString => System.Environment.GetEnvironmentVariable("AZURE_BLOB_CONNECTION_STRING");
        private static string Container => System.Environment.GetEnvironmentVariable("AZURE_BLOB_CONTAINER");

        [Fact(Skip = "Requires Azure Storage credentials")]
        public async Task AzureBlobKeyValueStore_SetAndGet_Works()
        {
            var store = new KeyValueStore.AzureBlobKeyValueStore(ConnectionString, Container);
            await store.SetAsync("foo", "bar");
            var value = await store.GetAsync("foo");
            Assert.Equal("bar", value);
        }

        [Fact(Skip = "Requires Azure Storage credentials")]
        public async Task AzureBlobKeyValueStore_Delete_RemovesKey()
        {
            var store = new KeyValueStore.AzureBlobKeyValueStore(ConnectionString, Container);
            await store.SetAsync("foo", "bar");
            await store.DeleteAsync("foo");
            var value = await store.GetAsync("foo");
            Assert.Null(value);
        }
    }

    public class S3KeyValueStoreTests
    {
        // These tests require a real AWS S3 bucket and credentials.
        // To enable, set the following environment variables:
        // AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, AWS_BUCKET, AWS_REGION
        private static string AccessKey => System.Environment.GetEnvironmentVariable("AWS_ACCESS_KEY_ID");
        private static string SecretKey => System.Environment.GetEnvironmentVariable("AWS_SECRET_ACCESS_KEY");
        private static string Bucket => System.Environment.GetEnvironmentVariable("AWS_BUCKET");
        private static string Region => System.Environment.GetEnvironmentVariable("AWS_REGION");

        [Fact(Skip = "Requires AWS credentials")]
        public async Task S3KeyValueStore_SetAndGet_Works()
        {
            var store = new KeyValueStore.S3KeyValueStore(AccessKey, SecretKey, Bucket, Region);
            await store.SetAsync("foo", "bar");
            var value = await store.GetAsync("foo");
            Assert.Equal("bar", value);
        }

        [Fact(Skip = "Requires AWS credentials")]
        public async Task S3KeyValueStore_Delete_RemovesKey()
        {
            var store = new KeyValueStore.S3KeyValueStore(AccessKey, SecretKey, Bucket, Region);
            await store.SetAsync("foo", "bar");
            await store.DeleteAsync("foo");
            var value = await store.GetAsync("foo");
            Assert.Null(value);
        }
    }
}
