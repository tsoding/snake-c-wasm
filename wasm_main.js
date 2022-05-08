'use strict';

let app = document.getElementById("app");
let ctx = app.getContext("2d");
let wasm = null;
let iota = 0;

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
    const r = ((color>>(0*8))&0xFF).toString(16).padStart(2, '0');
    const g = ((color>>(1*8))&0xFF).toString(16).padStart(2, '0');
    const b = ((color>>(2*8))&0xFF).toString(16).padStart(2, '0');
    const a = ((color>>(3*8))&0xFF).toString(16).padStart(2, '0');
    return "#"+r+g+b+a;
}

function platform_fill_rect(x, y, w, h, color) {
    ctx.fillStyle = color_hex(color); 
    ctx.fillRect(x, y, w, h);
}

function platform_stroke_rect(x, y, w, h, color) {
    ctx.strokeStyle = color_hex(color); 
    ctx.strokeRect(x, y, w, h);
}

iota = 0;
const ALIGN_LEFT   = iota++;
const ALIGN_RIGHT  = iota++;
const ALIGN_CENTER = iota++;
const ALIGN_NAMES = ["left", "right", "center"];

function platform_fill_text(x, y, text_ptr, size, color, align) {
    const buffer = wasm.instance.exports.memory.buffer;
    const text = cstr_by_ptr(buffer, text_ptr);
    ctx.fillStyle = color_hex(color);
    ctx.font = size+"px AnekLatin";
    ctx.textAlign = ALIGN_NAMES[align];
    ctx.fillText(text, x, y);
}

function platform_panic(file_path_ptr, line, message_ptr) {
    const buffer = wasm.instance.exports.memory.buffer;
    const file_path = cstr_by_ptr(buffer, file_path_ptr);
    const message = cstr_by_ptr(buffer, message_ptr);
    console.error(file_path+":"+line+": "+message);
    // TODO: WASM platform_panic() does not halt the game
}

function platform_log(message_ptr) {
    const buffer = wasm.instance.exports.memory.buffer;
    const message = cstr_by_ptr(buffer, message_ptr);
    console.log(message);
}

let prev = null;
function loop(timestamp) {
    if (prev !== null) {
        wasm.instance.exports.game_update((timestamp - prev)*0.001);
        wasm.instance.exports.game_render();
    }
    prev = timestamp;
    window.requestAnimationFrame(loop);
}

WebAssembly.instantiateStreaming(fetch('game.wasm'), {
    env: {
        platform_fill_rect,
        platform_stroke_rect,
        platform_fill_text,
        platform_panic,
        platform_log
    }
}).then((w) => {
    wasm = w;

    wasm.instance.exports.game_init(app.width, app.height);

    document.addEventListener('keydown', (e) => {
        wasm.instance.exports.game_keydown(e.key.charCodeAt());
    });

    const buffer = wasm.instance.exports.memory.buffer;

    window.requestAnimationFrame(loop);
});
