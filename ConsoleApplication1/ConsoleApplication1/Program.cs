using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO.Ports;
using System.Threading;

namespace ConsoleApplication1
{
    class Program
    {
        static void Main(string[] args)
        {
            var _serialPort = new SerialPort("COM4", 14400, Parity.None, 8, StopBits.One);
            _serialPort.Open();
           
            do
            {
                var data = GetBytesFromBinaryString("10101010");
                Thread.Sleep(100);

                _serialPort.Write(data,0,data.Length);
                data = GetBytesFromBinaryString("10101101");

                Thread.Sleep(100);
                _serialPort.Write(data, 0, data.Length);
                data = GetBytesFromBinaryString("01100100");


                _serialPort.Write(data, 0, data.Length);

                Console.ReadKey();

                data = GetBytesFromBinaryString("10101010");
                Thread.Sleep(100);

                _serialPort.Write(data, 0, data.Length);
                data = GetBytesFromBinaryString("10101101");

                Thread.Sleep(100);
                _serialPort.Write(data, 0, data.Length);
                data = GetBytesFromBinaryString("00000001");


                _serialPort.Write(data, 0, data.Length);

                Console.WriteLine($"10101010 10101101 01100100 -> resp:{_serialPort.ReadExisting()}");



            } while (Console.ReadKey().Key != ConsoleKey.Escape);

        }


        public static Byte[] GetBytesFromBinaryString(String binary)
        {
            var list = new List<Byte>();

            for (int i = 0; i < binary.Length; i += 8)
            {
                String t = binary.Substring(i, 8);

                list.Add(Convert.ToByte(t, 2));
            }

            return list.ToArray();
        }
    }
}
