document.addEventListener("DOMContentLoaded", () => {
    console.log("Page loaded!");
    const h1 = document.querySelector("h1");
    h1.addEventListener("click", () => {
        alert("Hello from JavaScript!");
    });
});

