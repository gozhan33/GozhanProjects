using System;
using System.Threading.Tasks;
using MitreAttackData;
using OpenAIAgent;

namespace ConsoleDemo
{
    class Program
    {
        static async Task Main(string[] args)
        {
            // NOTE: Set your OpenAI API key here or via environment variable
            var openAiKey = Environment.GetEnvironmentVariable("OPENAI_API_KEY") ?? "YOUR_OPENAI_API_KEY";
            if (openAiKey == "YOUR_OPENAI_API_KEY")
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine("WARNING: OpenAI API key is not set. Please set the OPENAI_API_KEY environment variable.");
                Console.ResetColor();
                // Throw an exception to prevent accidental use in production:
                throw new InvalidOperationException("OpenAI API key is not set. Set the OPENAI_API_KEY environment variable.");
            }
            else
            {
                Console.WriteLine("Using OpenAI API Key: " + openAiKey.Substring(0, 10) + "****");
            }
            // Initialize the MITRE provider and OpenAI agent
            // This will use the default endpoint, which is suitable for most cases
            var mitreProvider = new MitreAttackDataProvider();
            var agent = new OpenAIAgent.OpenAIAgent(mitreProvider, openAiKey);

            Console.WriteLine("Enter your prompt (or press Enter for default):");
            var prompt = Console.ReadLine();
            if (string.IsNullOrWhiteSpace(prompt))
                prompt = "Tell me the recent MITRE attack techniques related to AI";

            Console.WriteLine("\nQuerying OpenAI agent...\n");
            var response = await agent.GetMitreAttackResponseAsync(prompt);
            Console.WriteLine("Response:\n" + response);
        }
    }
}
