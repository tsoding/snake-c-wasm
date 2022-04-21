let app = document.getElementById("app");
let ctx = app.getContext("2d");
let wasm = null;

const KEY_LEFT    = 0;
const KEY_RIGHT   = 1;
const KEY_UP      = 2;
const KEY_DOWN    = 3;
const KEY_RESTART = 4;

let keys = new Set();

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

function color_hex(color) {
    r = ((color>>(0*8))&0xFF).toString(16).padStart(2, '0');
    g = ((color>>(1*8))&0xFF).toString(16).padStart(2, '0');
    b = ((color>>(2*8))&0xFF).toString(16).padStart(2, '0');
    a = ((color>>(3*8))&0xFF).toString(16).padStart(2, '0');
    return "#"+r+g+b;
}

function platform_fill_rect(x, y, w, h, color) {
    ctx.fillStyle = color_hex(color); 
    ctx.fillRect(x, y, w, h);
}

function platform_draw_text(x, y, text_ptr, size, color) {
    const buffer = wasm.instance.exports.memory.buffer;
    const text = cstr_by_ptr(buffer, text_ptr);
    ctx.fillStyle = color_hex(color);
    ctx.font = size+"px serif";
    ctx.fillText(text, x, y);
}

function platform_panic(file_path_ptr, line, message_ptr) {
    const buffer = wasm.instance.exports.memory.buffer;
    const file_path = cstr_by_ptr(buffer, file_path_ptr);
    const message = cstr_by_ptr(buffer, message_ptr);
    console.error(file_path+":"+line+": "+message);
}

function platform_log(message_ptr) {
    const buffer = wasm.instance.exports.memory.buffer;
    const message = cstr_by_ptr(buffer, message_ptr);
    console.log(message);
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
        platform_log,
        platform_draw_text,
    }
}).then((w) => {
    wasm = w;

    wasm.instance.exports.game_init();

    document.addEventListener('keydown', (e) => {
        switch (e.code) {
            case 'KeyA':  wasm.instance.exports.game_keydown(KEY_LEFT);    break;
            case 'KeyD':  wasm.instance.exports.game_keydown(KEY_RIGHT);   break;
            case 'KeyS':  wasm.instance.exports.game_keydown(KEY_DOWN);    break;
            case 'KeyW':  wasm.instance.exports.game_keydown(KEY_UP);      break;
            case 'Space': wasm.instance.exports.game_keydown(KEY_RESTART); break;
        }
    });

    const buffer = wasm.instance.exports.memory.buffer;
    const gi_ptr = wasm.instance.exports.game_info();
    const gi = new Uint32Array(buffer, gi_ptr, 2);
    app.width = gi[0];
    app.height = gi[1];

    window.requestAnimationFrame(loop);
});
