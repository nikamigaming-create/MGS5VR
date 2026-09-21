// Build a private local QAR overlay from a player's owned archive. Existing
// compressed/encrypted entry records are copied verbatim, never decoded/repacked.
// Layout/XOR constants: GzsTool (MIT); see licenses/GzsTool.txt.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
class CabinRatOverlay {
    static readonly uint[] X={0x41441043,0x11c22050,0xd05608c3,0x532c7319};
    static readonly uint[] D={0xbb8adedb,0x65229958,0x08453206,0x88121302,0x4c344955,0x2c02f10c,0x4887f823,0xf3818583};
    sealed class Record { public ulong hash; public byte[] raw; public long source,offset; }
    static void Require(bool v,string why){if(!v)throw new InvalidDataException(why);}
    static uint U(byte[] b,int p){return BitConverter.ToUInt32(b,p);}
    static void Put(byte[] b,int p,uint v){Buffer.BlockCopy(BitConverter.GetBytes(v),0,b,p,4);}
    static byte[] Read(FileStream s,long p,int n){Require(p>=0&&n>=0&&p+n<=s.Length,"Archive extent");s.Position=p;var b=new byte[n];int at=0;while(at<n){int k=s.Read(b,at,n-at);Require(k>0,"Short read");at+=k;}return b;}
    static long Align(long n,int s){return (n+(1L<<s)-1)&~((1L<<s)-1);}
    static string Hash(byte[] b){using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(b)).Replace("-","");}
    static Record NewRecord(ulong hash,string path){
        byte[] data=File.ReadAllBytes(path);Require(data.Length>48&&data.Length<16*1024*1024,"Package size");
        Require(System.Text.Encoding.ASCII.GetString(data,0,6)=="foxfpk","Expected FPK");
        var raw=new byte[data.Length+32];Put(raw,0,(uint)hash^X[0]);Put(raw,4,(uint)(hash>>32)^X[0]);
        Put(raw,8,(uint)data.Length^X[1]);Put(raw,12,(uint)data.Length^X[2]);
        byte[] md5;using(var m=MD5.Create())md5=m.ComputeHash(data);
        for(int i=0;i<4;i++)Put(raw,16+4*i,U(md5,4*i)^X[new[]{3,0,0,1}[i]]);
        for(int at=0;at<data.Length;at++){
            int block=at-at%8,index=(int)(2*(((ulong)(uint)hash+(uint)(block/11))%4));
            raw[32+at]=(byte)(data[at]^(byte)(D[index+(at%8>=4?1:0)]>>(8*(at%4))));
        }
        return new Record{hash=hash,raw=raw,source=-1};
    }
    static int Main(string[] a){try{
        Require(a.Length==6,"original.dat output.dat fpk-path fpk-hash fpkd-path fpkd-hash");
        Require(!File.Exists(a[1]),"Refuse overwrite");
        var additions=new[]{NewRecord(Convert.ToUInt64(a[3],16),a[2]),NewRecord(Convert.ToUInt64(a[5],16),a[4])};
        using(var input=new FileStream(a[0],FileMode.Open,FileAccess.Read,FileShare.Read)){
            var header=Read(input,0,32);Require(U(header,0)==0x52415153&&(U(header,24)^X[0])==1,"QAR v1 only");
            uint count=U(header,8)^X[1],unknown=U(header,12)^X[2];
            Require(count<100000&&unknown<100000,"Table bound");int shift=((U(header,4)^X[0])&0x800)!=0?12:10;
            var table=Read(input,32,checked((int)count*8));var extra=Read(input,32+table.Length,checked((int)unknown*16));
            var records=new List<Record>();var hashes=new HashSet<ulong>();
            for(int i=0;i<count;i++){
                int at=i*8;uint lo=U(table,at)^X[(i+at/5)%4],hi=U(table,at+4)^X[(i+(at+4)/5)%4];
                long pos=(long)((((ulong)hi<<32|lo)>>40)<<shift);var eh=Read(input,pos,32);
                ulong hash=((ulong)(U(eh,4)^X[0])<<32)|(U(eh,0)^X[0]);
                Require(hashes.Add(hash),"Duplicate original entry");uint size=U(eh,8)^X[1];
                Require(size<=256*1024*1024,"Entry too large");
                if(!additions.Any(r=>r.hash==hash))records.Add(new Record{hash=hash,source=pos,raw=Read(input,pos,checked((int)size+32))});
            }
            records.AddRange(additions);
            long next=Align(32+8L*records.Count+extra.Length,shift),first=next;
            foreach(var r in records){r.offset=next;next=Align(next+r.raw.Length,shift);Require((next>>shift)<(1L<<24),"QAR block limit");}
            Put(header,8,(uint)records.Count^X[1]);Put(header,16,(uint)(next>>shift)^X[3]);Put(header,20,(uint)first^X[0]);
            var newTable=new byte[records.Count*8];
            for(int i=0;i<records.Count;i++){
                var r=records[i];ulong section=((ulong)(r.offset>>shift)<<40)|((r.hash&255)<<32)|((r.hash>>32)&0xffffffff);
                int at=i*8;Put(newTable,at,(uint)section^X[(i+at/5)%4]);Put(newTable,at+4,(uint)(section>>32)^X[(i+(at+4)/5)%4]);
            }
            using(var output=new FileStream(a[1],FileMode.CreateNew,FileAccess.ReadWrite,FileShare.None)){
                output.Write(header,0,32);output.Write(newTable,0,newTable.Length);output.Write(extra,0,extra.Length);
                foreach(var r in records){output.Position=r.offset;output.Write(r.raw,0,r.raw.Length);}output.SetLength(next);output.Flush();
                foreach(var r in records)Require(Hash(Read(output,r.offset,r.raw.Length))==Hash(r.raw),"Post-write record mismatch");
            }
            Console.WriteLine("Preserved {0} original records byte-for-byte; added/replaced only 2 cabin packages; bytes={1}",records.Count-2,next);
        }
        return 0;
    }catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}}
}
