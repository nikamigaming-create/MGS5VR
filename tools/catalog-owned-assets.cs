// Read-only installed QAR/FPK inventory. No game execution or loose retail payloads.
using System;
using System.IO;
using System.Text;
using System.Linq;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Globalization;
using GzsTool.Core.Qar;
using GzsTool.Core.Fpk;

public static class MgsOwnedCatalog
{
    static string J(string value) {
        if (value == null) return "null";
        var result = new StringBuilder("\"");
        foreach (char c in value) {
            if (c == '"' || c == '\\') result.Append('\\').Append(c);
            else if (c < 32) result.Append("\\u").Append(((int)c).ToString("x4"));
            else result.Append(c);
        }
        return result.Append('"').ToString();
    }
    static string Hex(byte[] value) { return BitConverter.ToString(value).Replace("-", "").ToLowerInvariant(); }
    static string Number(float value) { return value.ToString("R", CultureInfo.InvariantCulture); }
    static string Ext(string value) { return Path.GetExtension(value ?? "").ToLowerInvariant(); }
    static void Range(byte[] b, long at, long size) {
        if (at < 0 || size < 0 || at > b.Length || size > b.Length-at) throw new InvalidDataException("Asset extent exceeds payload");
    }
    static ushort U16(byte[] b, long at) { Range(b,at,2); return BitConverter.ToUInt16(b,(int)at); }
    static uint U32(byte[] b, long at) { Range(b,at,4); return BitConverter.ToUInt32(b,(int)at); }
    static float F32(byte[] b, long at) { Range(b,at,4); return BitConverter.ToSingle(b,(int)at); }
    static byte[] Read(Stream source, int size) {
        if (size < 0 || size > 64*1024*1024) throw new InvalidDataException("Model exceeds 64 MiB inspection bound");
        var b=new byte[size]; int at=0;
        while (at<size) { int n=source.Read(b,at,size-at); if(n<=0) throw new EndOfStreamException(); at+=n; }
        return b;
    }
    static string Model(byte[] b) {
        if (b.Length<64 || U32(b,0)!=0x4c444d46 || Math.Abs(F32(b,4)-2.04f)>.02f)
            throw new InvalidDataException("Unsupported FMDL header");
        long table=(long)BitConverter.ToUInt64(b,8); uint n0=U32(b,32),n1=U32(b,36),base0=U32(b,40),base1=U32(b,48);
        if(n0>64 || n1>32) throw new InvalidDataException("Unsupported FMDL section count");
        Range(b,table,n0*8+n1*12);
        var sections=new Dictionary<ushort,Tuple<ushort,uint>>();
        for(uint i=0;i<n0;i++) { long at=table+i*8; sections.Add(U16(b,at),Tuple.Create(U16(b,at+2),U32(b,at+4))); }
        long bufferBase=-1;
        for(uint i=0;i<n1;i++) { long at=table+n0*8+i*12; if(U32(b,at)==2) {bufferBase=base1+U32(b,at+4);Range(b,bufferBase,U32(b,at+8));} }
        if(bufferBase<0 || !sections.ContainsKey(3) || !sections.ContainsKey(9) || !sections.ContainsKey(10) || !sections.ContainsKey(14))
            throw new InvalidDataException("Missing FMDL position sections");
        var buffers=new List<uint>(); var info=sections[14];
        for(int i=0;i<info.Item1;i++) buffers.Add(U32(b,base0+info.Item2+i*16+8));
        float[] min={float.PositiveInfinity,float.PositiveInfinity,float.PositiveInfinity},max={float.NegativeInfinity,float.NegativeInfinity,float.NegativeInfinity};
        int vertices=0; long indices=0; var meshes=sections[3];
        for(int i=0;i<meshes.Item1;i++) {
            long mesh=base0+meshes.Item2+i*48; int count=U16(b,mesh+10); indices+=U32(b,mesh+20);
            int first=U16(b,base0+sections[9].Item2+i*8+4); long format=base0+sections[10].Item2+first*8;
            Range(b,format,8); int buffer=b[(int)format],stride=b[(int)format+2];
            if(buffer>=buffers.Count || stride<12) throw new InvalidDataException("Invalid FMDL position format");
            long at=bufferBase+buffers[buffer]+U32(b,format+4); Range(b,at,(long)count*stride);
            for(int v=0;v<count;v++) for(int axis=0;axis<3;axis++) {
                float x=F32(b,at+v*stride+axis*4); if(float.IsNaN(x)||float.IsInfinity(x)) throw new InvalidDataException("Nonfinite FMDL position");
                min[axis]=Math.Min(min[axis],x); max[axis]=Math.Max(max[axis],x);
            }
            vertices+=count;
        }
        if(vertices==0) throw new InvalidDataException("Empty FMDL positions");
        int bones=sections.ContainsKey(0)?sections[0].Item1:0;
        return "{\"meshes\":"+meshes.Item1+",\"vertices\":"+vertices+",\"triangles\":"+(indices/3)+",\"bones\":"+bones+
            ",\"bounds_native\":{\"min\":["+String.Join(",",min.Select(Number))+"],\"max\":["+String.Join(",",max.Select(Number))+"]},\"animation_binding\":\"not_checked\"}";
    }
    public static void Scan(string game, string output) {
        string master=Path.Combine(game,"master"); if(!Directory.Exists(master)) throw new DirectoryNotFoundException(master);
        string staging=output+".tmp"; int dataFileCount=0,archiveCount=0,mediaCount=0,packageCount=0,assetCount=0,errorCount=0,modelCount=0;
        var analyzed=new Dictionary<string,string>(); var clock=System.Diagnostics.Stopwatch.StartNew();
        using(var sha=SHA256.Create()) using(var writer=new StreamWriter(staging,false,new UTF8Encoding(false))) {
            writer.WriteLine("{\"kind\":\"scan\",\"schema\":1,\"game\":"+J(Path.GetFullPath(game))+",\"utc\":"+J(DateTime.UtcNow.ToString("o"))+",\"payloads_exported\":false}");
            foreach(string archive in Directory.GetFiles(master,"*.dat",SearchOption.AllDirectories).OrderBy(p=>p,StringComparer.Ordinal)) {
                dataFileCount++; string relative=archive.Substring(master.Length+1).Replace('\\','/');
                var fi=new FileInfo(archive); int localPackages=0,localAssets=0;
                using(var input=File.Open(archive,FileMode.Open,FileAccess.Read,FileShare.Read)) {
                    try {
                        // Some hash-named DATs are ASF movies, not QAR archives.
                        // Match the mandatory ASF header GUID and bounded extent
                        // before handing a file to the archive decoder.
                        var signature=new byte[30]; int signatureSize=input.Read(signature,0,signature.Length);input.Position=0;
                        if(signatureSize==30&&Hex(signature.Take(16).ToArray())=="3026b2758e66cf11a6d900aa0062ce6c"
                            &&BitConverter.ToUInt64(signature,16)>=30&&BitConverter.ToUInt64(signature,16)<=(ulong)fi.Length
                            &&BitConverter.ToUInt32(signature,24)<=4096&&signature[28]==1&&signature[29]==2){
                            mediaCount++;writer.WriteLine("{\"kind\":\"media\",\"name\":"+J(relative)+",\"format\":\"asf\",\"bytes\":"+fi.Length
                                +",\"mtime_utc\":"+J(fi.LastWriteTimeUtc.ToString("o"))+"}");
                            Console.WriteLine(relative+": ASF media container (not an asset archive)");continue;
                        }
                        archiveCount++;
                        var qar=new QarFile(); qar.Read(input);
                        writer.WriteLine("{\"kind\":\"archive\",\"name\":"+J(relative)+",\"bytes\":"+fi.Length+",\"mtime_utc\":"+J(fi.LastWriteTimeUtc.ToString("o"))+",\"entries\":"+qar.Entries.Count+"}");
                        foreach(var entry in qar.Entries) {
                            string package=entry.Hash.ToString("x16"),path=entry.FileNameFound?entry.FilePath:null;
                            writer.WriteLine("{\"kind\":\"qar\",\"archive\":"+J(relative)+",\"id\":"+J(package)+",\"path\":"+J(path)+",\"ext\":"+J(Ext(path))+
                                ",\"offset\":"+entry.DataOffset+",\"bytes\":"+entry.UncompressedSize+",\"compressed\":"+(entry.Compressed?"true":"false")+"}");
                            string ext=Ext(path); if(ext!=".fpk" && ext!=".fpkd" && (entry.Hash>>56)!=0x52) continue;
                            try {
                                // Inspect in memory; never materialize an archive tree on disk.
                                if(entry.DataOffset<32) throw new InvalidDataException("Short QAR payload header");
                                input.Position=entry.DataOffset-32; var header=new byte[32]; if(input.Read(header,0,32)!=32) throw new EndOfStreamException();
                                uint extent=BitConverter.ToUInt32(header,12)^3495299267U;
                                if(extent>256*1024*1024) throw new InvalidDataException("Package exceeds 256 MiB inspection bound");
                                using(var payload=entry.Export(input).DataStream()) {
                                    if(payload.Length!=extent) throw new InvalidDataException("QAR decoded extent differs");
                                    var fpk=new FpkFile(); fpk.Read(payload); packageCount++; localPackages++;
                                    foreach(var asset in fpk.Entries) {
                                        var fp=asset.FilePathFpkString; string name=fp.ValueResolved?asset.FilePath:null,md5=Hex(asset.Md5Hash);
                                        string extra=""; string assetExt=Ext(name);
                                        if(assetExt==".fmdl") {
                                            try {
                                                byte[] bytes; using(var modelStream=asset.Export(payload).DataStream()) bytes=Read(modelStream,asset.DataSize);
                                                string digest=Hex(sha.ComputeHash(bytes)),metadata;
                                                if(!analyzed.TryGetValue(digest,out metadata)) {metadata=Model(bytes);analyzed.Add(digest,metadata);modelCount++;}
                                                extra=",\"sha256\":"+J(digest)+",\"model\":"+metadata;
                                            } catch(Exception ex) {extra=",\"model_error\":"+J(ex.GetType().Name+": "+ex.Message);}
                                        }
                                        writer.WriteLine("{\"kind\":\"asset\",\"archive\":"+J(relative)+",\"package\":"+J(package)+",\"id\":"+J(md5)+",\"path\":"+J(name)+",\"ext\":"+J(assetExt)+
                                            ",\"name_source\":"+J(fp.ValueEncrypted?"dictionary":"stored")+",\"offset\":"+asset.DataOffset+",\"bytes\":"+asset.DataSize+extra+"}");
                                        assetCount++;localAssets++;
                                    }
                                    foreach(var reference in fpk.References)
                                        writer.WriteLine("{\"kind\":\"package_reference\",\"archive\":"+J(relative)+",\"package\":"+J(package)+",\"path\":"+J(reference.FilePath)+"}");
                                }
                            } catch(Exception ex) {errorCount++;writer.WriteLine("{\"kind\":\"error\",\"archive\":"+J(relative)+",\"package\":"+J(package)+",\"message\":"+J(ex.GetType().Name+": "+ex.Message)+"}");}
                        }
                    } catch(Exception ex) {errorCount++;writer.WriteLine("{\"kind\":\"error\",\"archive\":"+J(relative)+",\"message\":"+J(ex.GetType().Name+": "+ex.Message)+"}");}
                }
                writer.Flush(); Console.WriteLine(relative+": "+localPackages+" packages, "+localAssets+" asset occurrences");
            }
            writer.WriteLine("{\"kind\":\"complete\",\"data_files\":"+dataFileCount+",\"archives\":"+archiveCount+",\"media_files\":"+mediaCount+",\"packages\":"+packageCount+",\"asset_occurrences\":"+assetCount+",\"unique_models_inspected\":"+modelCount+",\"errors\":"+errorCount+",\"seconds\":"+clock.Elapsed.TotalSeconds.ToString("F3",CultureInfo.InvariantCulture)+"}");
        }
        // A completed scan replaces metadata only; user annotations live separately.
        if(File.Exists(output)) File.Delete(output); File.Move(staging,output);
        Console.WriteLine("Catalog scan: "+packageCount+" packages, "+assetCount+" asset occurrences, "+errorCount+" unresolved package/archive errors -> "+output);
    }
}
