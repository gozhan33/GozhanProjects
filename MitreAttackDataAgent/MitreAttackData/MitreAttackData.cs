using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;

namespace MitreAttackData
{
    public class AttackTechnique
    {
        public string? Id { get; set; }
        public string? Name { get; set; }
        public string? Description { get; set; }
        public List<string>? Tags { get; set; }
    }

    public interface IMitreAttackDataProvider
    {
        Task<IEnumerable<AttackTechnique>> GetAllTechniquesAsync();
        Task<IEnumerable<AttackTechnique>> SearchTechniquesAsync(string keyword);
    }

    public class MitreAttackDataProvider : IMitreAttackDataProvider
    {
        private const string MitreAttackJsonUrl = "https://raw.githubusercontent.com/mitre/cti/master/enterprise-attack/enterprise-attack.json";
        private List<AttackTechnique>? _techniques;

        public async Task<IEnumerable<AttackTechnique>> GetAllTechniquesAsync()
        {
            if (_techniques == null)
                await LoadTechniquesAsync();
            return _techniques ?? new List<AttackTechnique>();
        }

        public async Task<IEnumerable<AttackTechnique>> SearchTechniquesAsync(string keyword)
        {
            if (_techniques == null)
                await LoadTechniquesAsync();
            keyword = keyword?.ToLowerInvariant() ?? string.Empty;
            return _techniques?.FindAll(t =>
                (t.Name?.ToLowerInvariant().Contains(keyword) ?? false) ||
                (t.Description?.ToLowerInvariant().Contains(keyword) ?? false) ||
                (t.Tags != null && t.Tags.Exists(tag => tag?.ToLowerInvariant().Contains(keyword) ?? false))
            ) ?? new List<AttackTechnique>();
        }

        private async Task LoadTechniquesAsync()
        {
            using var httpClient = new HttpClient();
            var json = await httpClient.GetStringAsync(MitreAttackJsonUrl);
            var doc = JsonDocument.Parse(json);
            var techniques = new List<AttackTechnique>();
            foreach (var obj in doc.RootElement.GetProperty("objects").EnumerateArray())
            {
                if (obj.GetProperty("type").GetString() == "attack-pattern")
                {
                    var technique = new AttackTechnique
                    {
                        Id = obj.TryGetProperty("external_references", out var refs) && refs.GetArrayLength() > 0 ?
                            refs[0].GetProperty("external_id").GetString() : null,
                        Name = obj.GetProperty("name").GetString(),
                        Description = obj.TryGetProperty("description", out var desc) ? desc.GetString() : string.Empty,
                        Tags = new List<string>()
                    };
                    if (obj.TryGetProperty("x_mitre_platforms", out var platforms))
                    {
                        foreach (var p in platforms.EnumerateArray())
                        {
                            var tag = p.GetString();
                            if (tag != null) technique.Tags.Add(tag);
                        }
                    }
                    techniques.Add(technique);
                }
            }
            _techniques = techniques;
        }
    }
}
