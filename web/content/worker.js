importScripts('puplang.js');

let module = null;

const sendResult = (type, data, msg) => self.postMessage({ type, ...data, name: msg.name, op: msg.op, mode: msg.mode, ex: data });
const sendError = (msg, err) => sendResult('error', { msg: String(err) }, msg);

async function run_endecode_ops(msg) {
  if (!module) {
    self.postMessage({ type: 'error', msg: 'worker not ready', name: msg.name, op: msg.op, mode: msg.mode });
    return;
  }

  const isEncode = msg.type === 'encode';
  const isText = msg.mode === 'text';

  try {
    let out;
    if (isText) {
      const fn = isEncode ? 'puplang_encode' : 'puplang_decode';
      out = module.ccall(fn, 'string', ['string', 'string'], [msg.content, '']);
    } else {
      const inPath = '/tmp/w_in.txt';
      const outPath = '/tmp/w_out.txt';

      const buffer = await msg.file.arrayBuffer();
      const byteArray = new Uint8Array(buffer);

      module.FS.writeFile(inPath, byteArray);

      const fn = isEncode ? 'puplang_encode_file' : 'puplang_decode_file';
      module.ccall(fn, null, ['string', 'string'], [inPath, outPath]);

      const rawBytes = module.FS.readFile(outPath);
      out = new Uint8Array(rawBytes);

      module.FS.unlink(inPath);
      module.FS.unlink(outPath);
    }

    const err = module.ccall('puplang_last_error', 'string', [], []);
    if (err !== '') {
      self.postMessage({ type: 'error', msg: err, name: msg.name, op: msg.op, mode: msg.mode });
      return;
    }

    if (out instanceof Uint8Array) {
      self.postMessage(
        { type: 'result', out: out, name: msg.name, op: msg.op, mode: msg.mode },
        [out.buffer]
      );
    } else {
      self.postMessage({ type: 'result', out: out, name: msg.name, op: msg.op, mode: msg.mode });
    }
  } catch (ex) {
    self.postMessage({ type: 'kms', msg: String(ex), ex: ex, op: msg.op });
  }
}

function init() {
  if (module)
    self.postMessage({ type: 'error', msg: 'worker already initialized' });
  createPuplangModule().then(function (m) {
    module = m;

    // forward progress updates back to the page
    module.puplangProgressSink = function (v) {
      self.postMessage({ type: 'progress', value: v });
    };

    self.postMessage({ type: 'ready' });
  });
  return;
}

function apply_settings(msg) {
  if (module) module.ccall(msg.fn, null, [msg.argType], [msg.val]);
}

self.onmessage = async function (e) { // handle messages from the main page
  const msg = e.data;
  if (msg.type === "init") {
    init();
  }

  // apply a settings change from the ui
  if (msg.type === "set") {
    apply_settings(msg);
  }

  // run an encode or decode job
  if (msg.type === "encode" || msg.type === "decode") {
    run_endecode_ops(msg);
  }
};