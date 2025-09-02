document.addEventListener("DOMContentLoaded", () => {
  const postForm = document.getElementById("postForm");
  postForm.addEventListener("submit", async (e) => {
    e.preventDefault();
    const formData = new FormData(postForm);
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
      document.getElementById("responsePost").textContent = text;
    } catch (err) {
      console.error(err);
      document.getElementById("responsePost").textContent = "Error sending POST";
    }
  });

  const deleteForm = document.getElementById("deleteForm");
  deleteForm.addEventListener("submit", async (e) => {
    e.preventDefault();
    const formData = new FormData(deleteForm);
    const filename = formData.get("filename");

    try {
      const res = await fetch("/" + encodeURIComponent(filename), {
        method: "DELETE",
      });
      const text = await res.text();
      document.getElementById("responseDelete").textContent = text;
    } catch (err) {
      console.error(err);
      document.getElementById("responseDelete").textContent = "Error sending DELETE";
    }
  });

  const browseButton = document.getElementById("browseButton");
  browseButton.addEventListener("click", async (e) => {
    window.location.href = "/anacondazz2";
  });  
});
