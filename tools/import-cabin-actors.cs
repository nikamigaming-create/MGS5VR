// Native cabin package augmentation from the player's owned TPP archives.
// No retail payload is embedded. Other QAR records remain byte-for-byte intact.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

internal static partial class OwnedAssets
{
    sealed class CabinPackage
    {
        public byte[] Header;
        public readonly Dictionary<string, byte[]> Files = new Dictionary<string, byte[]>(StringComparer.Ordinal);
        public readonly List<string> References = new List<string>();
    }
    static string FpkName(byte[] bytes, int field)
    {
        Require(U32(bytes, field + 4) == 0 && U32(bytes, field + 12) == 0, "FPK name exceeds supported extent");
        int at = checked((int)U32(bytes, field)), count = checked((int)U32(bytes, field + 8));
        Range(bytes, at, (long)count + 1);
        Require(count > 0 && count < 1024 && bytes[at + count] == 0, "Invalid FPK filename");
        string name = Encoding.ASCII.GetString(bytes, at, count);
        Require(name.StartsWith("/Assets/", StringComparison.Ordinal) && !name.Contains("..") && !name.Contains("\\"), "Unsupported encrypted or unsafe FPK path");
        return name;
    }
    static CabinPackage ParseCabinPackage(byte[] bytes)
    {
        Require(bytes.Length >= 48 && Encoding.ASCII.GetString(bytes, 0, 6) == "foxfpk", "Expected native FPK");
        Require(U32(bytes, 10) == bytes.Length && U32(bytes, 32) == 2, "Unsupported FPK header");
        int count = checked((int)U32(bytes, 36)), refs = checked((int)U32(bytes, 40));
        Require(count <= 4096 && refs <= 256, "FPK entry count bound");
        Range(bytes, 48, count * 48L + refs * 16L);
        var result = new CabinPackage { Header = Slice(bytes, 0, 48) };
        for (int i = 0; i < count; ++i)
        {
            int at = 48 + i * 48;
            Require(U32(bytes, at + 4) == 0 && U32(bytes, at + 12) == 0, "FPK data extent exceeds bound");
            string name = FpkName(bytes, at + 16);
            using (var md5 = MD5.Create()) Require(md5.ComputeHash(Encoding.ASCII.GetBytes(name)).SequenceEqual(Slice(bytes, at + 32, 16)), "FPK name digest differs");
            Require(!result.Files.ContainsKey(name), "Duplicate FPK path");
            result.Files.Add(name, Slice(bytes, checked((int)U32(bytes, at)), checked((int)U32(bytes, at + 8))));
        }
        for (int i = 0; i < refs; ++i) result.References.Add(FpkName(bytes, 48 + count * 48 + i * 16));
        return result;
    }
    static int Align16(int n) { return checked((n + 15) & ~15); }
    static byte[] WriteCabinPackage(CabinPackage package)
    {
        var files = package.Files.ToArray();
        byte[] header = (byte[])package.Header.Clone();
        Put32(header, 36, (uint)files.Length); Put32(header, 40, (uint)package.References.Count);
        int metadata = checked(48 + files.Length * 48 + package.References.Count * 16);
        using (var stream = new MemoryStream())
        {
            stream.SetLength(metadata); stream.Position = metadata;
            byte[] table = new byte[metadata - 48];
            for (int i = 0; i < files.Length + package.References.Count; ++i)
            {
                string name = i < files.Length ? files[i].Key : package.References[i - files.Length];
                int at = i < files.Length ? i * 48 + 16 : files.Length * 48 + (i - files.Length) * 16;
                byte[] text = Encoding.ASCII.GetBytes(name);
                Put32(table, at, (uint)stream.Position); Put32(table, at + 8, (uint)text.Length);
                stream.Write(text, 0, text.Length); stream.WriteByte(0);
                if (i < files.Length) using (var md5 = MD5.Create()) Buffer.BlockCopy(md5.ComputeHash(text), 0, table, i * 48 + 32, 16);
            }
            for (int i = 0; i < files.Length; ++i)
            {
                stream.Position = Align16(checked((int)stream.Position));
                Put32(table, i * 48, (uint)stream.Position); Put32(table, i * 48 + 8, (uint)files[i].Value.Length);
                stream.Write(files[i].Value, 0, files[i].Value.Length);
            }
            stream.SetLength(Align16(checked((int)stream.Position)));
            Put32(header, 10, (uint)stream.Length); stream.Position = 0;
            stream.Write(header, 0, header.Length); stream.Write(table, 0, table.Length);
            byte[] result = stream.ToArray();
            var verified = ParseCabinPackage(result);
            Require(verified.References.SequenceEqual(package.References) && verified.Files.Count == package.Files.Count, "FPK reference verification failed");
            foreach (var file in package.Files) Require(verified.Files[file.Key].SequenceEqual(file.Value), "FPK payload verification failed");
            return result;
        }
    }
    static void AddCabinAsset(CabinPackage package, string path, byte[] data)
    {
        byte[] prior;
        if (package.Files.TryGetValue(path, out prior)) Require(prior.SequenceEqual(data), "Existing modified cabin asset preserved: " + path);
        else package.Files.Add(path, data);
    }
    static void GroupCabinResource(CabinPackage package, string path, string extension, params string[] followingTypes)
    {
        // FPKD resources must retain the native type-group load order. A
        // parts entry appended after Lua parsed offline but its native FilePtr
        // remained null during rat initialization. Moving only these three
        // entries into existing groups fixed the same-DLL cold-load failure.
        var rows = package.Files.ToList();
        int from = rows.FindIndex(p => p.Key == path);
        Require(from >= 0, "Missing authored cabin dependency: " + path);
        var entry = rows[from]; rows.RemoveAt(from);
        int to = rows.FindLastIndex(p => p.Key.EndsWith(extension, StringComparison.Ordinal));
        if (to >= 0) ++to;
        else to = rows.FindIndex(p => followingTypes.Any(ext => p.Key.EndsWith(ext, StringComparison.Ordinal)));
        Require(to >= 0, "Unsupported native cabin resource ordering: " + path);
        rows.Insert(to, entry);
        package.Files.Clear();
        foreach (var item in rows) package.Files.Add(item.Key, item.Value);
    }
    static void GroupCabinDependencies(CabinPackage package)
    {
        GroupCabinResource(package, CabinDatasetPath, ".fox2", ".parts");
        GroupCabinResource(package, "/Assets/tpp/parts/chara/rat/rat2_main0_def_v00.parts", ".parts", ".vfxlb", ".vfx");
        GroupCabinResource(package, "/Assets/tpp/parts/chara/rat/Rat_Target.tgt", ".tgt", ".fsd", ".sdf", ".lua");
    }
    const string CabinDatasetPath = "/Assets/tpp/level/mission2/heli/h40050/mgs5vr_cabin_rats.fox2";
    static readonly int[] CabinMissions = { 40010, 40020, 40050 };
    static readonly ulong[] CabinFpkIds = { 0x5228f052e35de634UL, 0x522bad46ed61fbdfUL, 0x5228989bcda464b0UL };
    static readonly ulong[] CabinFpkdIds = { 0xed50f052e35de634UL, 0xed53ad46ed61fbdfUL, 0xed50989bcda464b0UL };
    static readonly string[] CabinChunks = { "chunk2.dat", "chunk4.dat", "chunk3.dat" };
    static readonly ModelSpec[] CabinOwned = {
        new ModelSpec("Assets/tpp/chara/rat/Scenes/rat2_main0_def.fmdl", "278cea2638f5daa4e1f40df2d9bca80a8acfc0d13fc73a46e36fe39af2607429"),
        new ModelSpec("Assets/tpp/motion/mtar/rat/TppRat_layers.mtar", "dbf72483e62433ffbc0219d3b2b2c3fde695780c1eb1759f26f34ff30b7a03ec"),
        new ModelSpec("Assets/tpp/parts/chara/rat/rat2_main0_def_v00.parts", "ee2095225efafec17cbab35c3489673b119ccad490bc882b675e793a860bc99b"),
        new ModelSpec("Assets/tpp/parts/chara/rat/Rat_Target.tgt", "54bddb46bcc9de2942bc40e17e4b8fc81d6fde6a562edf934fbe6a470de586b4")
    };
    sealed class CabinRecord { public ulong Id; public long At; public int Length; public byte[] Replacement; }
    static List<CabinRecord> CabinRecords(Stream input, out byte[] header, out byte[] extra, out int shift)
    {
        header = Read(input, 0, 32);
        Require(U32(header, 0) == 0x52415153 && (U32(header, 24) ^ Xor[0]) == 1, "Expected QAR v1");
        uint count = U32(header, 8) ^ Xor[1], other = U32(header, 12) ^ Xor[2];
        Require(count < 100000 && other == 0, "QAR table bound or unsupported auxiliary table");
        shift = ((U32(header, 4) ^ Xor[0]) & 0x800) != 0 ? 12 : 10;
        byte[] table = Read(input, 32, checked((int)count * 8));
        extra = Read(input, 32 + table.Length, checked((int)other * 16));
        var records = new List<CabinRecord>(); var seen = new HashSet<ulong>();
        for (int i = 0; i < count; ++i)
        {
            int at = i * 8;
            ulong word = ((ulong)(U32(table, at + 4) ^ Xor[(i + (at + 4) / 5) % 4]) << 32) | (U32(table, at) ^ Xor[(i + at / 5) % 4]);
            long position = checked((long)((word >> 40) << shift));
            byte[] entry = Read(input, position, 32);
            ulong id = ((ulong)(U32(entry, 4) ^ Xor[0]) << 32) | (U32(entry, 0) ^ Xor[0]);
            Require((word & 0xffffffffffUL) == ((id & 255) << 32 | id >> 32), "QAR lookup identity differs from record");
            uint size = U32(entry, 8) ^ Xor[1];
            Require(size <= 256 * 1024 * 1024 && size <= input.Length - position - 32 && seen.Add(id), "Invalid or duplicate QAR entry");
            records.Add(new CabinRecord { Id = id, At = position, Length = checked((int)size + 32) });
        }
        return records;
    }
    static byte[] EncodeCabinRecord(ulong id, byte[] bytes)
    {
        byte[] result = new byte[bytes.Length + 32];
        Put32(result, 0, (uint)id ^ Xor[0]); Put32(result, 4, (uint)(id >> 32) ^ Xor[0]);
        Put32(result, 8, (uint)bytes.Length ^ Xor[1]); Put32(result, 12, (uint)bytes.Length ^ Xor[2]);
        using (var md5 = MD5.Create()) {
            byte[] hash = md5.ComputeHash(bytes); int[] keys = { 3, 0, 0, 1 };
            for (int i = 0; i < 4; ++i) Put32(result, 16 + i * 4, U32(hash, i * 4) ^ Xor[keys[i]]);
        }
        for (int at = 0; at < bytes.Length; ++at) {
            int block = at - at % 8, key = (int)(2 * (((ulong)(uint)id + (uint)(block / 11)) % 4));
            result[32 + at] = (byte)(bytes[at] ^ (byte)(Decode[key + (at % 8 >= 4 ? 1 : 0)] >> (8 * (at % 4))));
        }
        return result;
    }
    static long CabinAlign(long n, int shift) { return (n + (1L << shift) - 1) & ~((1L << shift) - 1); }
    static void WriteCabinOverlay(string inputPath, string outputPath, Dictionary<ulong, byte[]> replacements)
    {
        Require(!File.Exists(outputPath), "Refuse overwrite of staged archive");
        using (var input = File.OpenRead(inputPath))
        {
            byte[] header, extra; int shift;
            var records = CabinRecords(input, out header, out extra, out shift);
            foreach (var item in replacements) {
                var record = records.Find(r => r.Id == item.Key);
                if (item.Value == null) { if (record != null) records.Remove(record); continue; }
                if (record == null) { record = new CabinRecord { Id = item.Key }; records.Add(record); }
                record.Replacement = EncodeCabinRecord(item.Key, item.Value); record.Length = record.Replacement.Length;
            }
            byte[] table = new byte[records.Count * 8];
            long next = CabinAlign(32L + table.Length + extra.Length, shift), first = next;
            var offsets = new List<long>();
            for (int i = 0; i < records.Count; ++i) {
                var r = records[i]; offsets.Add(next);
                Require((next >> shift) < (1L << 24), "QAR block limit");
                ulong word = ((ulong)(next >> shift) << 40) | ((r.Id & 255) << 32) | (r.Id >> 32);
                int at = i * 8; Put32(table, at, (uint)word ^ Xor[(i + at / 5) % 4]); Put32(table, at + 4, (uint)(word >> 32) ^ Xor[(i + (at + 4) / 5) % 4]);
                next = CabinAlign(next + r.Length, shift);
            }
            Put32(header, 8, (uint)records.Count ^ Xor[1]); Put32(header, 16, (uint)(next >> shift) ^ Xor[3]); Put32(header, 20, (uint)first ^ Xor[0]);
            using (var output = new FileStream(outputPath, FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None)) {
                output.Write(header, 0, header.Length); output.Write(table, 0, table.Length); output.Write(extra, 0, extra.Length);
                byte[] buffer = new byte[1024 * 1024];
                for (int i = 0; i < records.Count; ++i) {
                    var r = records[i]; output.Position = offsets[i];
                    if (r.Replacement != null) output.Write(r.Replacement, 0, r.Replacement.Length);
                    else { input.Position = r.At; int remaining = r.Length; while (remaining > 0) { int n = input.Read(buffer, 0, Math.Min(buffer.Length, remaining)); Require(n > 0, "Short original record"); output.Write(buffer, 0, n); remaining -= n; } }
                }
                output.SetLength(next); output.Flush(true);
                // Verify every preserved raw record, including compression/encryption.
                for (int i = 0; i < records.Count; ++i) {
                    var r = records[i]; int done = 0;
                    while (done < r.Length) {
                        int n = Math.Min(buffer.Length, r.Length - done);
                        byte[] expected = r.Replacement == null ? Read(input, r.At + done, n) : Slice(r.Replacement, done, n);
                        Require(expected.SequenceEqual(Read(output, offsets[i] + done, n)), "Post-write raw record verification failed"); done += n;
                    }
                }
            }
        }
    }
    static void PrepareCabin(string game, string output, bool remove = false)
    {
        game = Path.GetFullPath(game); output = Path.GetFullPath(output);
        output = SafeTarget(Path.GetDirectoryName(output), Path.GetFileName(output));
        using (var executable = File.OpenRead(SafeTarget(game, "mgsvtpp.exe"))) using (var sha = SHA256.Create())
            Require(BitConverter.ToString(sha.ComputeHash(executable)).Replace("-", "").ToLowerInvariant() == "085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45", "Unsupported owned game executable");
        Require(!File.Exists(output) && !output.StartsWith(game + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "Prepare into a fresh review stage outside the game");
        string master = Path.Combine(game, "master"), patch = Path.Combine(master, "0", "00.dat");
        SafeTarget(game, "master/0/00.dat");
        foreach (string chunk in CabinChunks) SafeTarget(game, "master/" + chunk);
        byte[] dataset = Convert.FromBase64String(CabinDataset), routes = Convert.FromBase64String(CabinRoutes);
        Dictionary<ulong, byte[]> present;
        using (var input = File.OpenRead(patch)) {
            byte[] header, extra; int shift;
            var wanted = new HashSet<ulong>(CabinRecords(input, out header, out extra, out shift).Select(r => r.Id).Where(id => CabinFpkIds.Contains(id) || CabinFpkdIds.Contains(id) || id == 0x52293a7e6ddb74dcUL || id == 0xed513a7e6ddb74dcUL));
            present = ReadArchive(input, wanted);
        }
        // The retail patch archive can update the animal's parts/target data.
        // Resolve exactly the same highest-priority native package as the game.
        var donor = ReadArchive(Path.Combine(master, "chunk2.dat"), 0x52293a7e6ddb74dcUL, 0xed513a7e6ddb74dcUL);
        foreach (ulong id in donor.Keys.ToArray()) if (present.ContainsKey(id)) donor[id] = present[id];
        var animals = new Dictionary<string, byte[]>();
        for (int i = 0; i < CabinOwned.Length; ++i) animals.Add("/" + CabinOwned[i].Path, ReadModel(donor[i < 2 ? 0x52293a7e6ddb74dcUL : 0xed513a7e6ddb74dcUL], CabinOwned[i]));
        var replacements = new Dictionary<ulong, byte[]>();
        for (int scene = 0; scene < CabinMissions.Length; ++scene) {
            var native = ReadArchive(Path.Combine(master, CabinChunks[scene]), CabinFpkIds[scene], CabinFpkdIds[scene]);
            foreach (ulong id in new[] { CabinFpkIds[scene], CabinFpkdIds[scene] }) {
                bool fpk = id == CabinFpkIds[scene];
                var package = ParseCabinPackage(present.ContainsKey(id) ? present[id] : native[id]);
                if (remove) {
                    if (!present.ContainsKey(id)) continue;
                    if (!present.ContainsKey(CabinFpkdIds[scene]) || !ParseCabinPackage(present[CabinFpkdIds[scene]]).Files.ContainsKey(CabinDatasetPath)) continue;
                    var original = ParseCabinPackage(native[id]);
                    string routePath = "/Assets/tpp/pack/mission2/heli/h" + CabinMissions[scene] + "/h" + CabinMissions[scene] + "_area.frt";
                    byte[] current;
                    if (fpk) {
                        Require(package.Files.TryGetValue(routePath, out current) && current.SequenceEqual(routes), "Modified cabin routes preserved during removal");
                        if (original.Files.ContainsKey(routePath)) package.Files[routePath] = original.Files[routePath]; else package.Files.Remove(routePath);
                    } else {
                        if (!package.Files.TryGetValue(CabinDatasetPath, out current)) continue;
                        Require(current.SequenceEqual(dataset) || Hash(current) == "a575100a5db02c7554acdcaf9396f531ec42b3cbfdf18e10226ab8c60168bb28", "Modified cabin dataset preserved during removal");
                        package.Files.Remove(CabinDatasetPath);
                    }
                    // Native payloads can be shared by a preexisting overlay.
                    // Retire only our exact authored dataset/routes; unreferenced
                    // native model/motion/parts bytes are harmless and not owned
                    // by this module for deletion.
                    bool sameAsBase = package.References.SequenceEqual(original.References) && package.Files.Count == original.Files.Count && original.Files.All(f => package.Files.ContainsKey(f.Key) && package.Files[f.Key].SequenceEqual(f.Value));
                    replacements.Add(id, sameAsBase ? null : WriteCabinPackage(package));
                    continue;
                }
                foreach (var animal in animals.Where(a => fpk == (a.Key.EndsWith(".fmdl", StringComparison.Ordinal) || a.Key.EndsWith(".mtar", StringComparison.Ordinal)))) {
                    byte[] priorAnimal;
                    if (package.Files.TryGetValue(animal.Key, out priorAnimal) && !priorAnimal.SequenceEqual(animal.Value)) {
                        string priorHash = Hash(priorAnimal);
                        bool originalParts = animal.Key == "/Assets/tpp/parts/chara/rat/rat2_main0_def_v00.parts" && priorHash == "be32b29f964ad46c34225e1a41d327c071b375524e68aed72f2f1d2fa07265c9";
                        bool originalTarget = animal.Key == "/Assets/tpp/parts/chara/rat/Rat_Target.tgt" && priorHash == "712b157932515dbcc857f12fcfa0c798cd3e4b89219ff3623a9f580c0c9e89ed";
                        bool legacyCabin = CabinMissions[scene] == 40050 &&
                            ((animal.Key == "/Assets/tpp/parts/chara/rat/rat2_main0_def_v00.parts" && priorHash == "ddeee425f999e7055753ab4249d493ca5cc8a2ecb4c66fada27bf8e0a086ba6a") ||
                             (animal.Key == "/Assets/tpp/parts/chara/rat/Rat_Target.tgt" && priorHash == "b779eb1c41571a886fdc2c712208ef66fb624db81b2f44c761bcebeb668bf54c"));
                        Require(originalParts || originalTarget || legacyCabin, "Existing modified native animal payload preserved: " + animal.Key);
                        // Preserve an already installed known native payload;
                        // priority resolution selects only newly added files.
                    } else AddCabinAsset(package, animal.Key, animal.Value);
                }
                if (fpk) {
                    string path = "/Assets/tpp/pack/mission2/heli/h" + CabinMissions[scene] + "/h" + CabinMissions[scene] + "_area.frt";
                    byte[] prior;
                    if (package.Files.TryGetValue(path, out prior)) Require(prior.SequenceEqual(routes) || (prior.Length == 28 && U32(prior, 0) == 0x54554f52 && U16(prior, 6) == 0), "Existing helicopter routes preserved; cannot replace a nonempty route table");
                    package.Files[path] = routes;
                } else {
                    byte[] prior;
                    if (package.Files.TryGetValue(CabinDatasetPath, out prior)) Require(prior.SequenceEqual(dataset) || Hash(prior) == "a575100a5db02c7554acdcaf9396f531ec42b3cbfdf18e10226ab8c60168bb28", "Unknown cabin dataset preserved");
                    package.Files[CabinDatasetPath] = dataset;
                    GroupCabinDependencies(package);
                }
                replacements.Add(id, WriteCabinPackage(package));
            }
        }
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        WriteCabinOverlay(patch, output, replacements);
        // Re-decode the completed archive independently of the raw write pass.
        var final = ReadArchive(output, replacements.Where(p => p.Value != null).Select(p => p.Key).ToArray());
        foreach (var entry in replacements.Where(p => p.Value != null)) Require(final[entry.Key].SequenceEqual(entry.Value), "Completed cabin package verification failed");
        Console.WriteLine(remove ? "Prepared removal of native cabin actor additions; unrelated native package contents preserved. No installed file or save changed."
            : "Prepared native rat datasets for helicopter scenes 40010, 40020 and 40050; all original QAR records and FPK assets/references preserved. No installed file or save changed.");
    }
    static void CabinSelfTest()
    {
        // This is the authored source compiler's complete output, not a
        // retail asset hash. Empty-string spelling and serialized graph
        // identities must survive edits; normalizing them changed runtime
        // data even though XML readers and the offline decoder accepted it.
        Require(Hash(Convert.FromBase64String(CabinDataset)) == "116afcc1a92c6fe29497d585ee96b95f8669052af7f0d511511ee342b424ddf7", "Authored cabin graph metadata changed; require compiler roundtrip and cold-scene validation");
        byte[] header = new byte[48]; Buffer.BlockCopy(Encoding.ASCII.GetBytes("foxfpk\0win"), 0, header, 0, 10); Put32(header, 32, 2);
        var package = new CabinPackage { Header = header };
        package.Files.Add("/Assets/authored/original.bin", new byte[] { 3, 1, 4 }); package.References.Add("/Assets/authored/shared.fpk");
        AddCabinAsset(package, "/Assets/authored/new.bin", new byte[] { 1, 5 });
        byte[] encoded = WriteCabinPackage(package); var decoded = ParseCabinPackage(encoded);
        Require(decoded.Files.Count == 2 && decoded.References.Single() == package.References.Single(), "FPK append fixture");
        RejectFixture(() => AddCabinAsset(package, "/Assets/authored/new.bin", new byte[] { 9 }), "Modified existing asset refusal");
        RejectFixture(() => ParseCabinPackage(Slice(encoded, 0, encoded.Length - 1)), "Truncated FPK refusal");
        var grouped = new CabinPackage { Header = header };
        foreach (string path in new[] { "/Assets/keep.fox2", "/Assets/keep.parts", "/Assets/keep.vfx", "/Assets/keep.sdf", "/Assets/keep.lua",
            "/Assets/tpp/parts/chara/rat/rat2_main0_def_v00.parts", "/Assets/tpp/parts/chara/rat/Rat_Target.tgt", CabinDatasetPath })
            grouped.Files.Add(path, Encoding.ASCII.GetBytes(path));
        var payloads = grouped.Files.ToDictionary(p => p.Key, p => p.Value);
        GroupCabinDependencies(grouped);
        Require(grouped.Files.Keys.SequenceEqual(new[] { "/Assets/keep.fox2", CabinDatasetPath, "/Assets/keep.parts",
            "/Assets/tpp/parts/chara/rat/rat2_main0_def_v00.parts", "/Assets/keep.vfx", "/Assets/tpp/parts/chara/rat/Rat_Target.tgt", "/Assets/keep.sdf", "/Assets/keep.lua" }), "Native FPKD resource groups");
        Require(grouped.Files.All(p => p.Value.SequenceEqual(payloads[p.Key])), "Grouping preserves every payload");
        byte[] ordered = WriteCabinPackage(grouped); GroupCabinDependencies(grouped);
        Require(WriteCabinPackage(grouped).SequenceEqual(ordered), "Resource grouping is idempotent");
        byte[] record = EncodeCabinRecord(0x5228989bcda464b0UL, encoded);
        Require(DecodeEntry(Slice(record, 32, record.Length - 32), 0xcda464b0, (uint)encoded.Length).SequenceEqual(encoded), "QAR encoder independent decoder fixture");
        Require(CabinMissions.SequenceEqual(new[] { 40010, 40020, 40050 }) && CabinFpkIds.Distinct().Count() == 3 && CabinFpkdIds.Distinct().Count() == 3, "All supported helicopter variants");
        byte[] routes = Convert.FromBase64String(CabinRoutes);
        Require(U32(routes, 0) == 0x54554f52 && U16(routes, 6) == 2 && U32(routes, 28) == 0x86358b80 && U32(routes, 32) == 0xfb33a487, "Both native route identifiers");
        Console.WriteLine("Cabin importer fixtures passed: all 3 scenes, preserved FPK payloads/references, conflicts, truncation, QAR roundtrip and both routes.");
    }
}
