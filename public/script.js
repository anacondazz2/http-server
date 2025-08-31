document.addEventListener("DOMContentLoaded", () => {
    const form = document.getElementById("postForm");
    form.addEventListener("submit", async (e) => {
        e.preventDefault();
        const formData = new FormData(form);
        const filename = formData.get("filename");
        const bodyContent = formData.get("body");

        try {
            const res = await fetch("/" + encodeURIComponent(filename), {
                method: "POST",
                body: bodyContent,
                headers: {
                    "Content-Type": "text/plain"
                }
            });
            const text = await res.text();
            document.getElementById("response").textContent = text;
        } catch (err) {
            console.error(err);
            document.getElementById("response").textContent = "Error sending POST";
        }
    });
});

