const app = document.getElementById("app")
const ctx = app.getContext("2d");
console.log(ctx);

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
    wasm.then((w) => {
        const buffer = w.instance.exports.memory.buffer;
        const file_path = cstr_by_ptr(buffer, file_path_ptr);
        const message = cstr_by_ptr(buffer, message_ptr);
        console.error(file_path+":"+line+": "+message);
    })
}

const wasm = WebAssembly.instantiateStreaming(fetch('game.wasm'), {
    env: {
        platform_fill_rect,
        platform_panic,
    }
});

function loop() {
    wasm.then((w) => {
        w.instance.exports.game_update(1/60);
        w.instance.exports.game_render();
        window.requestAnimationFrame(loop);
    });
}

wasm.then((w) => {
    app.width = w.instance.exports.game_width();
    app.height = w.instance.exports.game_height();
    window.requestAnimationFrame(loop);
});
