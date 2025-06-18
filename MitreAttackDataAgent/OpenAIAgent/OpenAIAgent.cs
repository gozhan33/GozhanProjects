using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using MitreAttackData;
using KeyValueStore;

namespace OpenAIAgent
{
    public interface IOpenAIAgent
    {
        Task<string> GetMitreAttackResponseAsync(string prompt);
    }

    public class OpenAIAgent : IOpenAIAgent
    {
        private readonly IMitreAttackDataProvider _mitreProvider;
        private readonly string _openAIApiKey;
        private readonly string _openAIEndpoint;
        private readonly IKeyValueStore _cacheStore = new FileSystemKeyValueStore("mitre_cache");
        private static readonly TimeSpan CacheTtl = TimeSpan.FromDays(3);

        public OpenAIAgent(IMitreAttackDataProvider mitreProvider, string openAIApiKey, string openAIEndpoint = "https://api.openai.com/v1/chat/completions")
        {
            _mitreProvider = mitreProvider;
            _openAIApiKey = openAIApiKey;
            _openAIEndpoint = openAIEndpoint;
        }

        // For testing, allow subclassing and overriding
        protected OpenAIAgent(IMitreAttackDataProvider mitreProvider) : this(mitreProvider, "fake-key", "http://localhost") { }

        public virtual async Task<string> GetMitreAttackResponseAsync(string prompt)
        {
            var keywords = ExtractKeywords(prompt);
            var relevantTechniques = new List<AttackTechnique>();
            foreach (var keyword in keywords)
            {
                var cacheKey = $"mitre_{keyword.ToLowerInvariant()}";
                var cached = await _cacheStore.GetAsync(cacheKey);
                List<AttackTechnique> found;
                if (cached != null)
                {
                    var cachedObj = JsonSerializer.Deserialize<CachedTechniques>(cached);
                    if (cachedObj != null && cachedObj.Expiry > DateTime.UtcNow)
                    {
                        found = cachedObj.Techniques;
                        Console.WriteLine($"Cache hit for keyword '{keyword}': {found.Count} techniques found.");
                        if (found.Count == 0)
                        {
                            Console.WriteLine($"No techniques found for keyword '{keyword}' in cache.");
                        }
                    }
                    else
                    {
                        found = (await _mitreProvider.SearchTechniquesAsync(keyword)).ToList();
                        await _cacheStore.SetAsync(cacheKey, JsonSerializer.Serialize(new CachedTechniques { Techniques = found, Expiry = DateTime.UtcNow + CacheTtl }));
                    }
                }
                else
                {
                    found = (await _mitreProvider.SearchTechniquesAsync(keyword)).ToList();
                    await _cacheStore.SetAsync(cacheKey, JsonSerializer.Serialize(new CachedTechniques { Techniques = found, Expiry = DateTime.UtcNow + CacheTtl }));
                }
                relevantTechniques.AddRange(found);
            }
            var uniqueTechniques = new Dictionary<string, AttackTechnique>();
            foreach (var t in relevantTechniques)
            {
                if (t.Id != null && !uniqueTechniques.ContainsKey(t.Id))
                    uniqueTechniques[t.Id] = t;
            }
            var context = string.Join("\n", uniqueTechniques.Values);
            var systemPrompt = $"You are an expert on MITRE ATT&CK. Here are recent techniques related to your query: {context}.";

            var requestBody = new
            {
                model = "gpt-3.5-turbo",
                messages = new[]
                {
                    new { role = "system", content = systemPrompt },
                    new { role = "user", content = prompt }
                }
            };

            using var httpClient = new HttpClient();
            httpClient.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", _openAIApiKey);
            var content = new StringContent(JsonSerializer.Serialize(requestBody), Encoding.UTF8, "application/json");
            var response = await httpClient.PostAsync(_openAIEndpoint, content);
            response.EnsureSuccessStatusCode();
            var responseString = await response.Content.ReadAsStringAsync();
            using var doc = JsonDocument.Parse(responseString);
            var message = doc.RootElement.GetProperty("choices")[0].GetProperty("message").GetProperty("content").GetString();
            return message;
        }

        private static IEnumerable<string> ExtractKeywords(string prompt)
        {
            // Simple keyword extraction: split by space, remove stopwords, return unique words
            var stopwords = new HashSet<string> { "the", "is", "a", "an", "to", "of", "and", "in", "on", "for", "with", "about", "related", "recent", "techniques", "mitre", "attack", "attacks", "relevant", "att&ck", "tell", "me" };
            var words = prompt.Split(new[] { ' ', ',', '.', '?', '!' }, StringSplitOptions.RemoveEmptyEntries);
            var keywords = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var word in words)
            {
                var w = word.Trim().ToLowerInvariant();
                if (!stopwords.Contains(w) && w.Length > 2)
                    keywords.Add(w);
            }
            // If no keywords found, default to "AI"
            if (keywords.Count == 0)
                keywords.Add("AI");
            return keywords;
        }

        private class CachedTechniques
        {
            public List<AttackTechnique> Techniques { get; set; }
            public DateTime Expiry { get; set; }
        }
    }
}
