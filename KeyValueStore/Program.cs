using System;
using System.Threading.Tasks;

namespace KeyValueStore
{
    class Program
    {
        static async Task Main(string[] args)
        {
            Console.WriteLine("Running KeyValueStore basic test...");
            await KeyValueStoreBasicTests.RunBasicTest();
            Console.WriteLine("Basic test completed. If no assertion failed, everything works!");
        }
    }
}
