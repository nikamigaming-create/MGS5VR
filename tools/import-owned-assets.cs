// Local, read-only import from the player's own supported TPP archives.
// QAR/FPK layout and XOR decoding adapted from GzsTool (MIT, Atvaark 2015).
// FTEX layout adapted from FtexTool (MIT). See licenses/GzsTool.txt and FtexTool.txt.
// No game content is embedded; exact hashes identify the supported owned assets.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

internal static class OwnedAssets
{
    const int Limit = 16 * 1024 * 1024;
    const ulong Package = 0x522a4f25e0ac957bUL;
    sealed class ModelSpec
    {
        public readonly string Path, Hash;
        public ModelSpec(string path, string hash) { Path = path; Hash = hash; }
    }
    sealed class TextureSpec
    {
        public readonly string Path, Hash;
        public readonly ulong Base;
        public readonly ulong[] Streams;
        public TextureSpec(string path, string hash, ulong @base, params ulong[] streams)
        { Path = path; Hash = hash; Base = @base; Streams = streams; }
    }
    // These are all authored entries from the player's common collectible FPK.
    // cct is the game's collectible cassette; rdi is its collectible radio.
    static readonly ModelSpec[] Models =
    {
        new ModelSpec("Assets/tpp/item/tel/Scenes/tel0_main0_def.fmdl", "935739377e6e0b14eb7186e778e265e65c8e2f66d97b8909d74725800eb21011"),
        new ModelSpec("Assets/tpp/item/cct/Scenes/cct0_main1_def.fmdl", "58512a084176bbbc4a2f496d7d36d70aa1f33f4b67455120bb2328bff7ee1a98"),
        new ModelSpec("Assets/tpp/item/rdi/Scenes/rdi0_main0_def.fmdl", "3681e86a1b611cc33bc67d756b815d4899147532a3417d78e510cd20eda54028"),
        new ModelSpec("Assets/tpp/item/idr/Scenes/idr0_main0_def.fmdl", "6e450f67a423f83f9d42717fb6ed7a464aa7b914c4524fe7dd762fe9dffc3f50")
    };
    static readonly TextureSpec[] Textures =
    {
        new TextureSpec("Assets/tpp/item/tel/Pictures/tel0_main0_def_c00_bsm.dds", "7cb40d536f37faa66d8153ef04afe6a23331d5bce45908dc7aa3f63558f566b3",
            0x1568643e638c1c21UL, 0xb2c0643e638c1c21UL, 0xb570643e638c1c21UL, 0x5718643e638c1c21UL),
        new TextureSpec("Assets/tpp/item/cct/Pictures/cct0_main1_def_c00_bsm.dds", "7199b3148526ac7a4db75051762cd80808a678910a880198b381291d350d7f51",
            0x156b3dc7c2e4e39cUL, 0xb2c33dc7c2e4e39cUL, 0xb5733dc7c2e4e39cUL, 0x571b3dc7c2e4e39cUL),
        new TextureSpec("Assets/tpp/item/rdi/Pictures/rdi0_main0_def_c00_bsm.dds", "f8b62e027451ded9864e70c0f189f753c9ea4b85ae6f9f9c9124eaa8c6896f09",
            0x156857c368d00700UL, 0xb2c057c368d00700UL, 0xb57057c368d00700UL),
        new TextureSpec("Assets/tpp/item/idr/Pictures/idr0_main0_def_c00_bsm.dds", "6129bf4bf7ab5a43f0180465356be4735b5591c0fa77a96172377662a45e18e7",
            0x15686aa73786877cUL, 0xb2c06aa73786877cUL, 0xb5706aa73786877cUL, 0x57186aa73786877cUL)
    };
    static readonly uint[] Xor = { 0x41441043, 0x11c22050, 0xd05608c3, 0x532c7319 };
    static readonly uint[] Decode = { 0xbb8adedb, 0x65229958, 0x08453206, 0x88121302, 0x4c344955, 0x2c02f10c, 0x4887f823, 0xf3818583 };

    static void Require(bool ok, string reason) { if (!ok) throw new InvalidDataException(reason); }
    static void Range(byte[] b, long at, long size)
    { Require(at >= 0 && size >= 0 && at <= b.Length && size <= b.Length - at, "Truncated asset data"); }
    static uint U32(byte[] b, int at) { Range(b, at, 4); return BitConverter.ToUInt32(b, at); }
    static ushort U16(byte[] b, int at) { Range(b, at, 2); return BitConverter.ToUInt16(b, at); }
    static byte[] Slice(byte[] b, int at, int size)
    { Range(b, at, size); var result = new byte[size]; Buffer.BlockCopy(b, at, result, 0, size); return result; }
    static byte[] Read(Stream input, long at, int size)
    {
        Require(at >= 0 && size >= 0 && size <= Limit && at <= input.Length && size <= input.Length - at, "Invalid archive extent");
        input.Position = at; var result = new byte[size]; int done = 0;
        while (done < size) { int n = input.Read(result, done, size - done); Require(n > 0, "Short archive read"); done += n; }
        return result;
    }
    static string Hash(byte[] data)
    { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(data)).Replace("-", "").ToLowerInvariant(); }
    static byte[] Inflate(byte[] data, int expected, bool zlib)
    {
        Require(expected > 0 && expected <= Limit, "Invalid inflated size");
        int skip = zlib ? 2 : 0, tail = zlib ? 4 : 0;
        Require(data.Length >= skip + tail, "Truncated compressed stream");
        if (zlib) Require((data[0] & 15) == 8 && ((data[0] << 8) + data[1]) % 31 == 0 && (data[1] & 32) == 0, "Unsupported zlib header");
        var output = new byte[expected];
        using (var source = new MemoryStream(data, skip, data.Length - skip - tail, false))
        using (var inflater = new DeflateStream(source, CompressionMode.Decompress))
        {
            int count = 0;
            while (count < expected) { int n = inflater.Read(output, count, expected - count); Require(n > 0, "Short inflated data"); count += n; }
            Require(inflater.ReadByte() == -1, "Inflated data exceeds declared size");
        }
        if (zlib)
        {
            uint a = 1, b = 0; foreach (byte v in output) { a = (a + v) % 65521; b = (b + a) % 65521; }
            int end = data.Length - 4;
            uint checksum = ((uint)data[end] << 24) | ((uint)data[end + 1] << 16) | ((uint)data[end + 2] << 8) | data[end + 3];
            Require(checksum == (b << 16 | a), "Compressed asset checksum mismatch");
        }
        return output;
    }
    static byte[] DecodeEntry(byte[] data, uint hashLow, uint expected)
    {
        for (int at = 0; at < data.Length; at++)
        {
            int block = at - at % 8;
            int index = (int)(2 * (((ulong)hashLow + (uint)(block / 11)) % 4));
            uint mask = Decode[index + (at % 8 >= 4 ? 1 : 0)];
            data[at] ^= (byte)(mask >> (8 * (at % 4)));
        }
        bool compressed = expected != data.Length;
        uint magic = U32(data, 0);
        if (magic == 0xa0f8efe6 || magic == 0xe3f8efe6)
        {
            uint key = U32(data, 4); data = Slice(data, magic == 0xa0f8efe6 ? 8 : 16, data.Length - (magic == 0xa0f8efe6 ? 8 : 16));
            uint mask = key | (((key ^ 25974) & 65535) << 16), streamKey = unchecked(278 * key);
            for (int at = 0; at + 4 <= data.Length; at += 4)
            {
                var value = BitConverter.GetBytes(U32(data, at) ^ mask); Buffer.BlockCopy(value, 0, data, at, 4);
                mask = unchecked(streamKey + 48828125 * mask);
            }
        }
        if (compressed) data = Inflate(data, checked((int)expected), true);
        Require(data.Length == expected, "Decoded QAR size mismatch"); return data;
    }
    static Dictionary<ulong, byte[]> ReadArchive(Stream input, HashSet<ulong> wanted)
    {
        var header = Read(input, 0, 32);
        Require(U32(header, 0) == 0x52415153 && (U32(header, 24) ^ Xor[0]) == 1, "Only the supported TPP QAR v1 archives can be imported");
        uint count = U32(header, 8) ^ Xor[1]; Require(count <= 1000000, "Archive entry count is excessive");
        int shift = ((U32(header, 4) ^ Xor[0]) & 0x800) != 0 ? 12 : 10;
        var table = Read(input, 32, checked((int)count * 8));
        var result = new Dictionary<ulong, byte[]>();
        for (int i = 0; i < count; i++)
        {
            int at = i * 8;
            uint low = U32(table, at) ^ Xor[(i + at / 5) % 4];
            uint high = U32(table, at + 4) ^ Xor[(i + (at + 4) / 5) % 4];
            long position = (long)((((ulong)high << 32 | low) >> 40) << shift);
            var entry = Read(input, position, 32);
            ulong hash = (ulong)(U32(entry, 4) ^ Xor[0]) << 32 | (U32(entry, 0) ^ Xor[0]);
            if (!wanted.Contains(hash)) continue;
            Require(!result.ContainsKey(hash), "Duplicate requested archive entry");
            // TPP v1 stores packed length first, inflated length second.
            // Reading the second as packed length also consumes archive padding.
            uint stored = U32(entry, 8) ^ Xor[1], expected = U32(entry, 12) ^ Xor[2];
            Require(expected > 0 && expected <= Limit && stored >= 4 && stored <= Limit, "Requested asset exceeds import size limit");
            try { result.Add(hash, DecodeEntry(Read(input, position + 32, (int)stored), (uint)hash, expected)); }
            catch (InvalidDataException e) { throw new InvalidDataException("QAR entry " + hash.ToString("x16") + ": " + e.Message, e); }
        }
        if (result.Count != wanted.Count)
        {
            var missing = wanted.Where(hash => !result.ContainsKey(hash)).Select(hash => hash.ToString("x16"));
            throw new InvalidDataException("Required owned assets are missing from the archive: " + string.Join(",", missing));
        }
        return result;
    }
    static Dictionary<ulong, byte[]> ReadArchive(string path, params ulong[] wanted)
    { using (var input = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read)) return ReadArchive(input, new HashSet<ulong>(wanted)); }
    static byte[] ReadModel(byte[] package, ModelSpec model)
    {
        Require(package.Length >= 48 && Encoding.ASCII.GetString(package, 0, 6) == "foxfpk", "Invalid collectible FPK");
        uint count = U32(package, 36); Range(package, 48, (long)count * 48);
        byte[] wanted; using (var md5 = MD5.Create()) wanted = md5.ComputeHash(Encoding.ASCII.GetBytes("/" + model.Path));
        byte[] found = null;
        for (int i = 0; i < count; i++)
        {
            int at = 48 + i * 48;
            if (!Slice(package, at + 32, 16).SequenceEqual(wanted)) continue;
            Require(found == null, "Duplicate collectible model");
            found = Slice(package, checked((int)U32(package, at)), checked((int)U32(package, at + 8)));
        }
        Require(found != null && Hash(found) == model.Hash, "Owned model differs from the supported asset: " + model.Path); return found;
    }
    static byte[] DecodeTexture(Dictionary<ulong, byte[]> archive, TextureSpec texture)
    {
        byte[] info = archive[texture.Base]; Require(info.Length >= 64 && U32(info, 0) == 0x58455446 && U32(info, 4) == 0x4001eb85, "Expected FTEX 2.04");
        uint format = U16(info, 8), width = U16(info, 10), height = U16(info, 12), depth = U16(info, 14), mips = info[16];
        Require((format == 2 || format == 4) && width > 0 && width <= 4096 && height > 0 && height <= 4096 && depth <= 1 && mips > 0 && mips <= 13, "Unsupported collectible texture layout");
        Range(info, 64, mips * 16);
        using (var output = new MemoryStream()) using (var writer = new BinaryWriter(output))
        {
            uint blockBytes = format == 2 ? 8u : 16u;
            uint[] header = new uint[31]; header[0] = 124; header[1] = 0xa1007;
            header[2] = height; header[3] = width; header[4] = ((width + 3) / 4) * ((height + 3) / 4) * blockBytes; header[6] = mips;
            header[18] = 32; header[19] = 4; header[20] = format == 2 ? 0x31545844u : 0x35545844u; header[26] = 0x1000u | (mips > 1 ? 0x400008u : 0u);
            writer.Write(0x20534444); foreach (uint v in header) writer.Write(v);
            for (int mip = 0; mip < mips; mip++)
            {
                int at = 64 + mip * 16, offset = checked((int)U32(info, at));
                int expected = checked((int)U32(info, at + 4)), stored = checked((int)U32(info, at + 8));
                int number = info[at + 13], chunks = U16(info, at + 14);
                Require(info[at + 12] == mip && number >= 1 && number <= texture.Streams.Length, "Invalid mip stream reference");
                uint mipBytes = Math.Max(1u, (Math.Max(1u, width >> mip) + 3) / 4) * Math.Max(1u, (Math.Max(1u, height >> mip) + 3) / 4) * blockBytes;
                Require(expected == mipBytes, "Mip dimensions and block size disagree");
                byte[] source = archive[texture.Streams[number - 1]]; long start = output.Position;
                if (chunks == 0) { Require(stored == expected, "Raw mip size mismatch"); writer.Write(Slice(source, offset, stored)); }
                else
                {
                    Range(source, offset, (long)chunks * 8);
                    for (int c = 0; c < chunks; c++)
                    {
                        int chunk = offset + c * 8, packed = U16(source, chunk), unpacked = U16(source, chunk + 2);
                        int dataAt = checked(offset + (int)(U32(source, chunk + 4) & 0x7fffffff));
                        byte[] bytes = Slice(source, dataAt, packed);
                        if (packed != unpacked) bytes = Inflate(bytes, unpacked, true);
                        Require(output.Position - start + bytes.Length <= expected, "Oversized mip chunks"); writer.Write(bytes);
                    }
                }
                Require(output.Position - start == expected, "Decoded mip size mismatch");
            }
            var data = output.ToArray();
            var hash = Hash(data);
            Console.WriteLine("Owned texture " + texture.Path + " " + width + "x" + height + " mips=" + mips + " sha256=" + hash);
            Require(string.IsNullOrEmpty(texture.Hash) || hash == texture.Hash, "Imported texture differs from the supported asset: " + texture.Path);
            return data;
        }
    }
    static string SafeTarget(string root, string relative)
    {
        var path = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
        Require(path.StartsWith(root.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "Output escapes the import directory");
        for (string part = path; part != null; part = Path.GetDirectoryName(part))
            if (File.Exists(part) || Directory.Exists(part)) Require((File.GetAttributes(part) & FileAttributes.ReparsePoint) == 0, "Import does not follow linked output paths");
        return path;
    }
    static void Import(string game, string destination)
    {
        game = Path.GetFullPath(game); destination = Path.GetFullPath(destination);
        // Import only the minimum validated local files; the game archives are never changed.
        var package = ReadArchive(Path.Combine(game, "master", "chunk0.dat"), Package)[Package];
        var wanted = new HashSet<ulong>(Textures.SelectMany(texture => texture.Streams.Concat(new[] { texture.Base })));
        var textureArchive = ReadArchive(Path.Combine(game, "master", "texture0.dat"), wanted.ToArray());
        var outputs = new Dictionary<string, byte[]>();
        foreach (var model in Models) outputs.Add(SafeTarget(destination, model.Path), ReadModel(package, model));
        foreach (var texture in Textures) outputs.Add(SafeTarget(destination, texture.Path), DecodeTexture(textureArchive, texture));
        foreach (var file in outputs) if (File.Exists(file.Key))
        {
            Require(new FileInfo(file.Key).Length == file.Value.Length, "Existing modified asset was preserved: " + file.Key);
            Require(Hash(File.ReadAllBytes(file.Key)) == Hash(file.Value), "Existing modified asset was preserved: " + file.Key);
        }
        var created = new List<string>();
        try
        {
            foreach (var file in outputs)
            {
                if (File.Exists(file.Key)) continue;
                Directory.CreateDirectory(Path.GetDirectoryName(file.Key));
                using (var target = new FileStream(file.Key, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                { created.Add(file.Key); target.Write(file.Value, 0, file.Value.Length); target.Flush(true); }
            }
        }
        catch { foreach (string path in created) File.Delete(path); throw; }
        Console.WriteLine("Imported owned binocular, cassette, radio and iDroid materials locally. No archive or save was modified.");
    }
    static void SelfTest()
    {
        bool rejected = false; try { using (var s = new MemoryStream(new byte[32])) ReadArchive(s, new HashSet<ulong>()); } catch (InvalidDataException) { rejected = true; }
        Require(rejected, "QAR magic test");
        rejected = false; try { Slice(new byte[8], 4, 8); } catch (InvalidDataException) { rejected = true; }
        Require(rejected, "Extent test");
        rejected = false; try { ReadModel(new byte[48], Models[0]); } catch (InvalidDataException) { rejected = true; }
        Require(rejected, "FPK magic test");
        rejected = false; try { SafeTarget(Path.GetFullPath("owned-fixture"), "../outside"); } catch (InvalidDataException) { rejected = true; }
        Require(rejected, "Output containment test");
        byte[] compressed = Convert.FromBase64String("eJzLSM3JyQcABiwCFQ==");
        Require(Encoding.ASCII.GetString(Inflate(compressed, 5, true)) == "hello", "Zlib fixture");
        compressed[compressed.Length - 1] ^= 1;
        rejected = false; try { Inflate(compressed, 5, true); } catch (InvalidDataException) { rejected = true; }
        Require(rejected, "Zlib checksum test");
        Console.WriteLine("Owned-asset parser self-tests passed.");
    }
    static int Main(string[] args)
    {
        try
        {
            if (args.Length == 1 && args[0] == "--self-test") { SelfTest(); return 0; }
            if (args.Length != 2) { Console.Error.WriteLine("Usage: mgs5vr_import <owned TPP directory> <mod-local output directory>"); return 2; }
            Import(args[0], args[1]); return 0;
        }
        catch (Exception e) { Console.Error.WriteLine("Owned asset import failed: " + e.Message); return 1; }
    }
}
