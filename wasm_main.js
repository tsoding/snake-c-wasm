let app = document.getElementById("app");
let ctx = app.getContext("2d");
let wasm = null;

const KEY_LEFT  = 0;
const KEY_RIGHT = 1;
const KEY_UP    = 2;
const KEY_DOWN  = 3;

let keys = new Set();

document.addEventListener('keydown', (e) => {
    switch (e.code) {
        case 'KeyA': keys.add(KEY_LEFT);  break;
        case 'KeyD': keys.add(KEY_RIGHT); break;
        case 'KeyS': keys.add(KEY_DOWN);  break;
        case 'KeyW': keys.add(KEY_UP);    break;
    }
});

document.addEventListener('keyup', (e) => {
    switch (e.code) {
        case 'KeyA': keys.delete(KEY_LEFT);  break;
        case 'KeyD': keys.delete(KEY_RIGHT); break;
        case 'KeyS': keys.delete(KEY_DOWN);  break;
        case 'KeyW': keys.delete(KEY_UP);    break;
    }
});

function platform_fill_rect(x, y, w, h, color) {
    r = ((color>>(0*8))&0xFF).toString(16).padStart(2, '0');
    g = ((color>>(1*8))&0xFF).toString(16).padStart(2, '0');
    b = ((color>>(2*8))&0xFF).toString(16).padStart(2, '0');
    ctx.fillStyle = "#"+r+g+b;
    ctx.fillRect(x, y, w, h);
}

function cstrlen(mem, ptr) {
    let len = 0;
    while (mem[ptr] != 0) {
        len++;
        ptr++;
    }
    return len;
}

function cstr_by_ptr(mem_buffer, ptr) {
    const mem = new Uint8Array(mem_buffer);
    const len = cstrlen(mem, ptr);
    const bytes = new Uint8Array(mem_buffer, ptr, len);
    return new TextDecoder().decode(bytes);
}

function platform_panic(file_path_ptr, line, message_ptr) {
    const buffer = wasm.instance.exports.memory.buffer;
    const file_path = cstr_by_ptr(buffer, file_path_ptr);
    const message = cstr_by_ptr(buffer, message_ptr);
    console.error(file_path+":"+line+": "+message);
}

function platform_keydown(key) {
    return keys.has(key);
}

function loop() {
    wasm.instance.exports.game_update(1/60);
    wasm.instance.exports.game_render();
    window.requestAnimationFrame(loop);
}

WebAssembly.instantiateStreaming(fetch('game.wasm'), {
    env: {
        platform_fill_rect,
        platform_panic,
        platform_keydown,
    }
}).then((w) => {
    wasm = w;
    app.width = wasm.instance.exports.game_width();
    app.height = wasm.instance.exports.game_height();
    window.requestAnimationFrame(loop);
});
