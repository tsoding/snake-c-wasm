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

const wasm = WebAssembly.instantiateStreaming(fetch('game.wasm'), {
    env: {
        platform_fill_rect
    }
});

wasm.then((w) => {
    app.width = w.instance.exports.game_width();
    app.height = w.instance.exports.game_height();
    w.instance.exports.game_render();
});
