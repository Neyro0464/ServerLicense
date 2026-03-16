document.addEventListener('DOMContentLoaded', () => {
    const loginForm = document.getElementById('loginForm');
    const loginBtn = document.getElementById('loginBtn');
    const btnText = document.querySelector('.btn-text');
    const btnLoader = document.getElementById('btnLoader');
    const errorToast = document.getElementById('errorToast');
    const errorMessage = document.getElementById('errorMessage');

    // Check if already logged in
    fetch('/api/me').then(res => {
        if (res.ok) {
            window.location.href = '/index.html';
        }
    });

    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();

        errorToast.classList.add('hidden');
        btnText.classList.add('hidden');
        btnLoader.classList.remove('hidden');
        loginBtn.disabled = true;

        const formData = new FormData(loginForm);
        const data = Object.fromEntries(formData.entries());

        try {
            const response = await fetch('/api/login', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });

            if (!response.ok) {
                throw new Error('Invalid username or password');
            }

            window.location.href = '/index.html';
        } catch (error) {
            errorMessage.textContent = error.message;
            errorToast.classList.remove('hidden');
        } finally {
            btnText.classList.remove('hidden');
            btnLoader.classList.add('hidden');
            loginBtn.disabled = false;
        }
    });
});
