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

function platform_text_width(text_ptr, size) {
    const buffer = wasm.instance.exports.memory.buffer;
    const text = cstr_by_ptr(buffer, text_ptr);
    ctx.font = size+"px AnekLatin";
    return ctx.measureText(text).width;
}

function platform_fill_text(x, y, text_ptr, size, color) {
    const buffer = wasm.instance.exports.memory.buffer;
    const text = cstr_by_ptr(buffer, text_ptr);
    ctx.fillStyle = color_hex(color);
    ctx.font = size+"px AnekLatin";
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
let touchStartX = null;
let touchStartY = null;
let touchEndX = null;
let touchEndY = null;
let touchStartTimestamp = null;
function loop(timestamp) {
    if (prev !== null) {
        wasm.instance.exports.game_update((timestamp - prev)*0.001);
        wasm.instance.exports.game_render();
    }
    prev = timestamp;
    window.requestAnimationFrame(loop);
}

function mod(a, b) { return (a%b + b)%b }

WebAssembly.instantiateStreaming(fetch('game.wasm'), {
    env: {
        platform_fill_rect,
        platform_stroke_rect,
        platform_fill_text,
        platform_panic,
        platform_log,
        platform_text_width,
    }
}).then((w) => {
    wasm = w;

    wasm.instance.exports.game_init(app.width, app.height);

    document.addEventListener('keydown', (e) => {
        wasm.instance.exports.game_keydown(e.key.charCodeAt());
    });

    document.addEventListener("touchstart", (e) => {
        touchStartX = e.targetTouches[0].screenX;
        touchStartY = e.targetTouches[0].screenY;
        touchStartTimestamp = e.timeStamp;
    }, false);
    document.addEventListener('touchmove', (e) => {
        touchEndX = e.targetTouches[0].screenX;
        touchEndY = e.targetTouches[0].screenY;
    }, false);
    document.addEventListener('touchend', (e) => {
        const deltaX = touchEndX - touchStartX;
        const deltaY = touchEndY - touchStartY;
        const deltaTime = e.timeStamp - touchStartTimestamp;
        const distance = Math.sqrt(deltaX*deltaX + deltaY*deltaY);
        const intensity = distance / deltaTime;
        const angle = mod(Math.atan2(deltaY, deltaX), 2*Math.PI);
        const THRESHOLD = 0.2;
        if (intensity > THRESHOLD) {
            const QOP = Math.PI/4; // Quater of Pee
            if ((0*QOP <= angle && angle < 1*QOP) || (7*QOP <= angle && angle < 8*QOP)) {
                wasm.instance.exports.game_keydown('d'.charCodeAt());
            } else if (1*QOP <= angle && angle < 3*QOP) {
                wasm.instance.exports.game_keydown('s'.charCodeAt());
            } else if (3*QOP <= angle && angle < 5*QOP) {
                wasm.instance.exports.game_keydown('a'.charCodeAt());
            } else if (5*QOP <= angle && angle < 7*QOP) {
                wasm.instance.exports.game_keydown('w'.charCodeAt());
            }
        }
    }, false);

    const buffer = wasm.instance.exports.memory.buffer;

    window.requestAnimationFrame(loop);
});
