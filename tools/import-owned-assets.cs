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
    const ulong ShaderArchiveEntry = 0x2cfffeb10aca508dUL;
    const int ShaderBankBytes = 113613497;
    const string ShaderBankHash = "af0c5f3e4084122cf102cae2aef8e0b8c0bb85af8632464a2533832567f97886";
    const string MaterialShaderName = "fox3ddf_blin_4mt";
    const string MaterialShaderPath = "shaders/dx11/fox3ddf_blin_4mt-ps.dxbc";
    const int MaterialShaderBytes = 233604;
    const string MaterialShaderHash = "cabc964687cd259e0189b33968b090980aab7e12a9c603e8560536ae2a9e64fe";
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
        new TextureSpec("Assets/tpp/item/tel/Pictures/tel0_main0_def_nrm.dds", "285c9a61b02b5798e79a5013b49ce3344b1f196ae2a56814ab7db6bfb9028eb4",
            0x156a46fa1ab1bafdUL, 0xb2c246fa1ab1bafdUL, 0xb57246fa1ab1bafdUL, 0x571a46fa1ab1bafdUL),
        new TextureSpec("Assets/tpp/item/tel/Pictures/tel0_main0_def_srm.dds", "20f1d6b3c6e13c149a79fc3526a524f442fc3457bceaa5ce775579bd0d43b61d",
            0x156bd2b731b98391UL, 0xb2c3d2b731b98391UL, 0xb573d2b731b98391UL, 0x571bd2b731b98391UL),
        new TextureSpec("Assets/tpp/item/tel/Pictures/tel0_main0_def_mtm.dds", "88c9679a783ece860f6182d4444b0ad07b59771be2d1b0eedab166e4f18b3e13",
            0x1568ed887bb8a4d9UL, 0xb2c0ed887bb8a4d9UL, 0xb570ed887bb8a4d9UL, 0x5718ed887bb8a4d9UL),
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
    static byte[] Read(Stream input, long at, int size, int limit = Limit)
    {
        Require(at >= 0 && size >= 0 && size <= limit && at <= input.Length && size <= input.Length - at, "Invalid archive extent");
        input.Position = at; var result = new byte[size]; int done = 0;
        while (done < size) { int n = input.Read(result, done, size - done); Require(n > 0, "Short archive read"); done += n; }
        return result;
    }
    static string Hash(byte[] data)
    { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(data)).Replace("-", "").ToLowerInvariant(); }
    static byte[] Inflate(byte[] data, int expected, bool zlib, int limit = Limit)
    {
        Require(expected > 0 && expected <= limit, "Invalid inflated size");
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
    static byte[] DecodeEntry(byte[] data, uint hashLow, uint expected, int limit = Limit)
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
        if (compressed) data = Inflate(data, checked((int)expected), true, limit);
        Require(data.Length == expected, "Decoded QAR size mismatch"); return data;
    }
    static Dictionary<ulong, byte[]> ReadArchive(Stream input, HashSet<ulong> wanted, int entryLimit = Limit)
    {
        Require(entryLimit >= 1 && entryLimit <= ShaderBankBytes, "Invalid selective archive import limit");
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
            Require(expected > 0 && expected <= entryLimit && stored >= 4 && stored <= entryLimit, "Requested asset exceeds import size limit");
            // Only this exact owned FSOP entry may exceed the ordinary asset cap.
            if (expected > Limit || stored > Limit)
                Require(hash == ShaderArchiveEntry && expected == ShaderBankBytes && stored == expected,
                    "Unsupported large shader archive entry");
            try { result.Add(hash, DecodeEntry(Read(input, position + 32, (int)stored, entryLimit), (uint)hash, expected, entryLimit)); }
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
    // FSOP is an authored sequential name/VS/PS record stream. Only the
    // selected PS is copied; unrelated programs are never written to disk.
    static byte[] SelectFsopPixelShader(byte[] bank, string wanted)
    {
        int at = 0, records = 0; byte[] selected = null;
        while (at < bank.Length)
        {
            Require(++records <= 65536, "FSOP record count is excessive");
            int nameBytes = bank[at++];
            Require(nameBytes > 1, "Invalid FSOP name length"); Range(bank, at, nameBytes);
            Require(bank[at + nameBytes - 1] == 0, "Unterminated FSOP name");
            for (int i = 0; i < nameBytes - 1; i++)
                Require(bank[at + i] >= 32 && bank[at + i] <= 126, "Invalid FSOP ASCII name");
            string name = Encoding.ASCII.GetString(bank, at, nameBytes - 1); at += nameBytes;
            for (int stage = 0; stage < 2; stage++)
            {
                uint size = U32(bank, at); at += 4;
                Require(size <= Limit, "FSOP stage exceeds import size limit"); Range(bank, at, size);
                if (stage == 1 && name == wanted)
                {
                    Require(selected == null, "Duplicate requested FSOP shader");
                    Require(size > 0, "Requested FSOP pixel shader is empty");
                    selected = Slice(bank, at, (int)size);
                    for (int i = 0; i < selected.Length; i++) selected[i] ^= 0x9c;
                }
                at += (int)size;
            }
        }
        Require(selected != null, "Required owned FSOP pixel shader is missing: " + wanted);
        return selected;
    }
    static string ShaderString(byte[] data, uint offset)
    {
        Require(offset < data.Length, "Invalid shader reflection string offset");
        int start = (int)offset, end = start;
        while (end < data.Length && end - start <= 128 && data[end] != 0)
        { Require(data[end] >= 32 && data[end] <= 126, "Invalid shader reflection name"); end++; }
        Require(end < data.Length && end - start > 0 && end - start <= 128 && data[end] == 0,
            "Unterminated shader reflection name");
        return Encoding.ASCII.GetString(data, start, end - start);
    }
    static void ValidateSignature(byte[] signature, bool output)
    {
        int count = output ? 3 : 6;
        Require(U32(signature, 0) == count && U32(signature, 4) == 8, "Unexpected material shader signature count");
        Range(signature, 8, count * 24);
        string[] names = output ? new[] { "SV_Target", "SV_Target", "SV_Target" }
            : new[] { "SV_Position", "COLOR", "TEXCOORD", "TEXCOORD", "TEXCOORD", "TEXCOORD" };
        uint[] indices = output ? new uint[] { 0, 1, 2 } : new uint[] { 0, 0, 0, 5, 6, 7 };
        uint[] masks = output ? new uint[] { 15, 15, 15 } : new uint[] { 15, 15, 3, 7, 7, 7 };
        for (int i = 0; i < count; i++)
        {
            int row = 8 + i * 24; uint name = U32(signature, row);
            Require(name >= 8 + count * 24 && ShaderString(signature, name) == names[i]
                && U32(signature, row + 4) == indices[i] && U32(signature, row + 16) == i
                && U32(signature, row + 12) == 3 && (U32(signature, row + 20) & 255) == masks[i],
                "Unsupported material shader signature");
            Require(U32(signature, row + 8) == (!output && i == 0 ? 1u : 0u), "Unexpected material shader signature system value");
        }
    }
    static void ValidateMaterialShaderReflection(byte[] shader)
    {
        Require(shader.Length >= 32 && U32(shader, 0) == 0x43425844 && U32(shader, 24) == shader.Length,
            "Invalid material shader DXBC container");
        uint count = U32(shader, 28); Require(count >= 4 && count <= 32, "Invalid shader chunk count");
        Range(shader, 32, count * 4); var chunks = new Dictionary<string, byte[]>();
        var extents = new List<Tuple<int, int>>();
        for (int i = 0; i < count; i++)
        {
            int offset = checked((int)U32(shader, 32 + i * 4)); Range(shader, offset, 8);
            int size = checked((int)U32(shader, offset + 4)); Range(shader, offset + 8, size);
            Require(offset >= 32 + count * 4, "Shader chunk overlaps its header");
            string name = Encoding.ASCII.GetString(shader, offset, 4);
            Require(!chunks.ContainsKey(name), "Duplicate shader reflection chunk");
            chunks.Add(name, Slice(shader, offset + 8, size)); extents.Add(Tuple.Create(offset, offset + 8 + size));
        }
        int previous = 0;
        foreach (var extent in extents.OrderBy(value => value.Item1))
        { Require(extent.Item1 >= previous, "Overlapping shader chunks"); previous = extent.Item2; }
        foreach (string name in new[] { "RDEF", "ISGN", "OSGN", "SHEX" })
            Require(chunks.ContainsKey(name), "Missing material shader reflection chunk: " + name);
        byte[] program = chunks["SHEX"];
        Require(program.Length >= 8 && program.Length % 4 == 0 && U32(program, 0) == 0x50
            && U32(program, 4) == program.Length / 4, "Owned material shader must use native ps_5_0");
        ValidateSignature(chunks["ISGN"], false); ValidateSignature(chunks["OSGN"], true);
        byte[] reflection = chunks["RDEF"];
        Require(U32(reflection, 0) == 2 && U32(reflection, 8) == 11, "Unexpected material shader reflection counts");
        int cbAt = checked((int)U32(reflection, 4)), bindingAt = checked((int)U32(reflection, 12));
        Range(reflection, cbAt, 2 * 24); Range(reflection, bindingAt, 11 * 32);
        var buffers = new Dictionary<string, uint>();
        for (int i = 0; i < 2; i++)
        {
            int row = cbAt + i * 24; string name = ShaderString(reflection, U32(reflection, row));
            Require(!buffers.ContainsKey(name) && U32(reflection, row + 20) == 0, "Invalid material shader constant buffer");
            buffers.Add(name, U32(reflection, row + 12));
        }
        Require(buffers.ContainsKey("cPSSystem") && buffers["cPSSystem"] == 64
            && buffers.ContainsKey("cPSMaterial") && buffers["cPSMaterial"] == 128,
            "Material shader constant-buffer sizes differ from the native contract");
        string[] resources = { "g_sampler_diffuse", "g_sampler_normal", "g_sampler_srm", "g_samplerPoint_Wrap",
            "g_tex_diffuse", "g_tex_normal", "g_tex_srm", "g_tex_materialmap", "g_tex_mesh", "cPSSystem", "cPSMaterial" };
        uint[] slots = { 0, 1, 2, 8, 0, 1, 2, 3, 15, 0, 4 };
        var seen = new HashSet<string>();
        for (int i = 0; i < 11; i++)
        {
            int row = bindingAt + i * 32; string name = ShaderString(reflection, U32(reflection, row));
            int expected = Array.IndexOf(resources, name);
            Require(expected >= 0 && seen.Add(name), "Unexpected or duplicate material shader resource");
            uint type = expected < 4 ? 3u : expected < 9 ? 2u : 0u;
            Require(U32(reflection, row + 4) == type && U32(reflection, row + 20) == slots[expected]
                && U32(reflection, row + 24) == 1, "Material shader binding differs from the native contract");
            if (type == 2) Require(U32(reflection, row + 8) == 5 && U32(reflection, row + 12) == 4,
                "Material shader texture must be a native float Texture2D");
        }
    }
    static void ValidateMaterialShaderIdentity(byte[] shader)
    {
        Require(shader.Length == MaterialShaderBytes && Hash(shader) == MaterialShaderHash,
            "Owned material pixel shader differs from the supported asset");
        ValidateMaterialShaderReflection(shader);
    }
    static byte[] ReadMaterialShader(string game)
    {
        byte[] bank;
        using (var input = new FileStream(Path.Combine(game, "master", "data1.dat"), FileMode.Open, FileAccess.Read, FileShare.Read))
            bank = ReadArchive(input, new HashSet<ulong> { ShaderArchiveEntry }, ShaderBankBytes)[ShaderArchiveEntry];
        Require(bank.Length == ShaderBankBytes && Hash(bank) == ShaderBankHash,
            "Owned GrModel shader bank differs from the supported asset");
        byte[] shader = SelectFsopPixelShader(bank, MaterialShaderName);
        ValidateMaterialShaderIdentity(shader);
        Console.WriteLine("Owned shader " + MaterialShaderName + " ps_5_0 bytes=" + shader.Length + " sha256=" + MaterialShaderHash);
        return shader;
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
        outputs.Add(SafeTarget(destination, MaterialShaderPath), ReadMaterialShader(game));
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
        Console.WriteLine("Imported owned binocular, cassette, radio, iDroid materials and native material shader locally. No archive or save was modified.");
    }
    // These fixtures are independently authored metadata, not game bytecode.
    // The synthetic token stream cannot pass the production identity gate.
    static void Put32(byte[] bytes, int at, uint value)
    { Range(bytes, at, 4); Buffer.BlockCopy(BitConverter.GetBytes(value), 0, bytes, at, 4); }
    static uint FixtureName(MemoryStream stream, string name)
    {
        uint offset = checked((uint)stream.Position); byte[] bytes = Encoding.ASCII.GetBytes(name + "\0");
        stream.Write(bytes, 0, bytes.Length); return offset;
    }
    static byte[] FixtureSignature(bool output)
    {
        int count = output ? 3 : 6;
        string[] names = output ? new[] { "SV_Target", "SV_Target", "SV_Target" }
            : new[] { "SV_Position", "COLOR", "TEXCOORD", "TEXCOORD", "TEXCOORD", "TEXCOORD" };
        uint[] indices = output ? new uint[] { 0, 1, 2 } : new uint[] { 0, 0, 0, 5, 6, 7 };
        uint[] masks = output ? new uint[] { 15, 15, 15 } : new uint[] { 15, 15, 3, 7, 7, 7 };
        byte[] header = new byte[8 + count * 24]; Put32(header, 0, (uint)count); Put32(header, 4, 8);
        using (var stream = new MemoryStream())
        {
            stream.Write(header, 0, header.Length);
            for (int i = 0; i < count; i++)
            {
                int row = 8 + i * 24; Put32(header, row, FixtureName(stream, names[i]));
                Put32(header, row + 4, indices[i]); Put32(header, row + 8, !output && i == 0 ? 1u : 0u);
                Put32(header, row + 12, 3); Put32(header, row + 16, (uint)i); Put32(header, row + 20, masks[i]);
            }
            byte[] result = stream.ToArray(); Buffer.BlockCopy(header, 0, result, 0, header.Length); return result;
        }
    }
    static byte[] FixtureReflection()
    {
        const int cbAt = 28, bindingAt = cbAt + 48;
        byte[] header = new byte[bindingAt + 11 * 32];
        Put32(header, 0, 2); Put32(header, 4, cbAt); Put32(header, 8, 11); Put32(header, 12, bindingAt);
        string[] names = { "g_sampler_diffuse", "g_sampler_normal", "g_sampler_srm", "g_samplerPoint_Wrap",
            "g_tex_diffuse", "g_tex_normal", "g_tex_srm", "g_tex_materialmap", "g_tex_mesh", "cPSSystem", "cPSMaterial" };
        uint[] slots = { 0, 1, 2, 8, 0, 1, 2, 3, 15, 0, 4 };
        using (var stream = new MemoryStream())
        {
            stream.Write(header, 0, header.Length);
            for (int i = 0; i < names.Length; i++)
            {
                uint name = FixtureName(stream, names[i]); int row = bindingAt + i * 32;
                uint type = i < 4 ? 3u : i < 9 ? 2u : 0u;
                Put32(header, row, name); Put32(header, row + 4, type); Put32(header, row + 20, slots[i]); Put32(header, row + 24, 1);
                if (type == 2) { Put32(header, row + 8, 5); Put32(header, row + 12, 4); }
                if (i >= 9)
                { int buffer = cbAt + (i - 9) * 24; Put32(header, buffer, name); Put32(header, buffer + 12, i == 9 ? 64u : 128u); }
            }
            byte[] result = stream.ToArray(); Buffer.BlockCopy(header, 0, result, 0, header.Length); return result;
        }
    }
    static byte[] FixtureShader()
    {
        string[] tags = { "RDEF", "ISGN", "OSGN", "SHEX" };
        byte[] program = new byte[8]; Put32(program, 0, 0x50); Put32(program, 4, 2);
        byte[][] payloads = { FixtureReflection(), FixtureSignature(false), FixtureSignature(true), program };
        byte[] header = new byte[48]; Put32(header, 0, 0x43425844); Put32(header, 20, 1); Put32(header, 28, 4);
        using (var stream = new MemoryStream()) using (var writer = new BinaryWriter(stream))
        {
            writer.Write(header);
            for (int i = 0; i < tags.Length; i++)
            {
                while (stream.Position % 4 != 0) writer.Write((byte)0);
                Put32(header, 32 + i * 4, checked((uint)stream.Position));
                writer.Write(Encoding.ASCII.GetBytes(tags[i])); writer.Write(payloads[i].Length); writer.Write(payloads[i]);
            }
            byte[] result = stream.ToArray(); Put32(header, 24, (uint)result.Length);
            Buffer.BlockCopy(header, 0, result, 0, header.Length); return result;
        }
    }
    static byte[] FixtureFsop(params string[] names)
    {
        using (var stream = new MemoryStream()) using (var writer = new BinaryWriter(stream))
        {
            foreach (string name in names)
            {
                byte[] encoded = Encoding.ASCII.GetBytes(name + "\0"); writer.Write((byte)encoded.Length); writer.Write(encoded);
                writer.Write(0u); writer.Write(4u);
                foreach (byte value in Encoding.ASCII.GetBytes("DXBC")) writer.Write((byte)(value ^ 0x9c));
            }
            return stream.ToArray();
        }
    }
    static void RejectFixture(Action action, string name)
    {
        bool rejected = false; try { action(); } catch (InvalidDataException) { rejected = true; }
        Require(rejected, name);
    }
    static void ShaderSelfTest()
    {
        byte[] bank = FixtureFsop("ignored", MaterialShaderName, "also_ignored");
        Require(Encoding.ASCII.GetString(SelectFsopPixelShader(bank, MaterialShaderName)) == "DXBC", "Selective FSOP PS/XOR fixture");
        RejectFixture(() => SelectFsopPixelShader(FixtureFsop("ignored"), MaterialShaderName), "Missing FSOP program fixture");
        RejectFixture(() => SelectFsopPixelShader(FixtureFsop(MaterialShaderName, MaterialShaderName), MaterialShaderName), "Duplicate FSOP program fixture");
        RejectFixture(() => SelectFsopPixelShader(Slice(bank, 0, bank.Length - 1), MaterialShaderName), "Truncated FSOP extent fixture");
        bank = FixtureFsop(MaterialShaderName); bank[bank[0]] = 1;
        RejectFixture(() => SelectFsopPixelShader(bank, MaterialShaderName), "FSOP name termination fixture");
        bank = FixtureFsop(MaterialShaderName); Put32(bank, bank[0] + 1, Limit + 1u);
        RejectFixture(() => SelectFsopPixelShader(bank, MaterialShaderName), "Bounded FSOP stage fixture");
        byte[] shader = FixtureShader(); ValidateMaterialShaderReflection(shader);
        RejectFixture(() => ValidateMaterialShaderIdentity(shader), "Synthetic shader never receives owned identity");
        byte[] broken = (byte[])shader.Clone(); Put32(broken, 24, (uint)broken.Length - 1);
        RejectFixture(() => ValidateMaterialShaderReflection(broken), "DXBC declared-size fixture");
        broken = (byte[])shader.Clone(); Put32(broken, 36, U32(broken, 32));
        RejectFixture(() => ValidateMaterialShaderReflection(broken), "DXBC duplicate chunk fixture");
        broken = (byte[])shader.Clone(); Put32(broken, 32, (uint)broken.Length - 4);
        RejectFixture(() => ValidateMaterialShaderReflection(broken), "DXBC chunk extent fixture");
        broken = (byte[])shader.Clone(); Put32(broken, (int)U32(broken, 44) + 8, 0x10050);
        RejectFixture(() => ValidateMaterialShaderReflection(broken), "Native pixel profile fixture");
        broken = (byte[])shader.Clone(); Put32(broken, (int)U32(broken, 44) + 12, 999);
        RejectFixture(() => ValidateMaterialShaderReflection(broken), "Shader token-stream extent fixture");
        broken = (byte[])shader.Clone(); Put32(broken, (int)U32(broken, 40) + 8, 2);
        RejectFixture(() => ValidateMaterialShaderReflection(broken), "Three native output targets fixture");
        broken = (byte[])shader.Clone(); int reflection = (int)U32(broken, 32) + 8;
        Put32(broken, reflection + 28 + 12, 80);
        RejectFixture(() => ValidateMaterialShaderReflection(broken), "System buffer size fixture");
        broken = (byte[])shader.Clone(); Put32(broken, reflection + 76 + 7 * 32 + 20, 9);
        RejectFixture(() => ValidateMaterialShaderReflection(broken), "MTM binding slot fixture");
        broken = (byte[])shader.Clone(); Put32(broken, reflection + 76 + 4 * 32 + 12, 5);
        RejectFixture(() => ValidateMaterialShaderReflection(broken), "Texture2D reflection dimension fixture");
        Console.WriteLine("Owned shader parser fixtures passed: selective FSOP, missing/duplicate/truncated records, exact identity refusal, DXBC extents, profile, signatures, native buffer and material bindings.");
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
        ShaderSelfTest();
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
