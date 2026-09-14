"""Protected snapshots must not make cross-origin pixels readable."""
import base64
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import threading

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.fixture
def image_origin():
    png = base64.b64decode(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aD1sAAAAASUVORK5CYII="
    )

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            body = png if self.path == "/pixel.png" else b"<!doctype html><title>Origin control</title>"
            self.send_response(200)
            self.send_header("Content-Type", "image/png" if self.path == "/pixel.png" else "text/html")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server.server_port
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


@pytest.mark.asyncio
@pytest.mark.parametrize("mode", ["native", "override"])
async def test_canvas_snapshots_preserve_origin_taint(tmp_path, image_origin, mode):
    fixture, mock = canvas_control_files(tmp_path, mode)
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode="mock", skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
    ) as launch:
        page = launch["page"]
        await page.goto(f"http://127.0.0.1:{image_origin}/")
        observations = await page.evaluate(r"""async port => {
            async function load(host) {
                const img=new Image();
                await new Promise((resolve,reject)=>{
                    img.onload=resolve; img.onerror=()=>reject(new Error('image control failed'));
                    img.src=`http://${host}:${port}/pixel.png`;
                });
                return img;
            }
            const records=[];
            async function check(source, name, tainted) {
                async function operation(label, fn) {
                    try {await fn();records.push({name,label,tainted,error:null});}
                    catch(e) {records.push({name,label,tainted,error:e.name});}
                }
                const dom=document.createElement('canvas'); dom.width=dom.height=4;
                const d=dom.getContext('2d'); d.drawImage(source,0,0);
                await operation('read',()=>d.getImageData(0,0,4,4));
                await operation('dataURL',()=>dom.toDataURL());
                const off=new OffscreenCanvas(4,4); off.getContext('2d').drawImage(source,0,0);
                await operation('blob',()=>off.convertToBlob());
                for(const version of ['webgl','webgl2']) {
                    const gl=document.createElement('canvas').getContext(version);
                    if(!gl) throw new Error('missing '+version);
                    const texture=gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D,texture);
                    await operation(version,()=>{
                        gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,source);
                        if(gl.getError()!==gl.NO_ERROR) throw new Error('upload failed');
                    });
                    gl.deleteTexture(texture);
                }
            }
            for(const tainted of [false,true]) {
                const img=await load(tainted?'localhost':'127.0.0.1');
                const canvas=document.createElement('canvas'); canvas.width=canvas.height=4;
                canvas.getContext('2d').drawImage(img,0,0);
                const off=new OffscreenCanvas(4,4); off.getContext('2d').drawImage(img,0,0);
                await check(canvas,'canvas',tainted);
                await check(off,'offscreen',tainted);
                const bitmap=await createImageBitmap(canvas);
                try {await check(bitmap,'bitmap',tainted);} finally {bitmap.close();}
                const transferred=off.transferToImageBitmap();
                try {await check(transferred,'transferred',tainted);} finally {transferred.close();}
            }
            return records;
        }""", image_origin)
    assert len(observations) == 40
    for observation in observations:
        assert observation["error"] == ("SecurityError" if observation["tainted"] else None), observation
