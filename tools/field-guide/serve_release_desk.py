"""Local-only field-kit preview with byte ranges for reliable video seeking."""
import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re


class Handler(SimpleHTTPRequestHandler):
    def send_head(self):
        self.remaining = None
        value = self.headers.get('Range')
        path = Path(self.translate_path(self.path))
        if not value or not path.is_file():
            return super().send_head()
        match = re.fullmatch(r'bytes=(\d*)-(\d*)', value)
        stream = path.open('rb')
        size = path.stat().st_size
        try:
            if not match or not any(match.groups()):
                raise ValueError('Unsupported range')
            a, b = match.groups()
            start = int(a) if a else max(0, size-int(b))
            end = min(size-1, int(b)) if a and b else size-1
            if not 0 <= start <= end < size:
                raise ValueError('Range outside file')
        except ValueError:
            stream.close();self.send_response(416);self.send_header('Content-Range', f'bytes */{size}');self.end_headers();return None
        self.send_response(206)
        self.send_header('Content-type', self.guess_type(str(path)))
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Content-Range', f'bytes {start}-{end}/{size}')
        self.send_header('Content-Length', str(end-start+1))
        self.end_headers();stream.seek(start);self.remaining=end-start+1
        return stream

    def copyfile(self, source, output):
        if self.remaining is None:
            return super().copyfile(source, output)
        while self.remaining:
            block=source.read(min(self.remaining, 128*1024))
            if not block:break
            try:output.write(block)
            except (BrokenPipeError, ConnectionResetError):break
            self.remaining-=len(block)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',type=int,default=8766)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    server=ThreadingHTTPServer(('127.0.0.1',args.port),lambda *a,**kw:Handler(*a,directory=str(root),**kw))
    print(f'Local field kit: http://127.0.0.1:{args.port}/artifacts/field-guide-20260927/release-desk/',flush=True)
    server.serve_forever()
