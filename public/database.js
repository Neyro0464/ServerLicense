document.addEventListener('DOMContentLoaded', () => {
    const tableBody = document.getElementById('licensesTableBody');
    const errorToast = document.getElementById('errorToast');
    const errorMessage = document.getElementById('errorMessage');
    const logoutBtn = document.getElementById('logoutBtn');

    if (logoutBtn) {
        logoutBtn.addEventListener('click', () => {
            fetch('/api/logout', { method: 'POST' }).then(() => {
                window.location.href = '/login.html';
            });
        });
    }

    async function loadLicenses() {
        try {
            const response = await fetch('/api/licenses');

            if (!response.ok) {
                if (response.status === 401) {
                    window.location.href = '/login.html';
                    return;
                }
                throw new Error('Failed to fetch licenses from server.');
            }

            const data = await response.json();

            tableBody.innerHTML = ''; // clear loading

            if (data.length === 0) {
                tableBody.innerHTML = '<tr><td colspan="8" style="text-align: center;">No licenses found.</td></tr>';
                return;
            }

            data.forEach(license => {
                const tr = document.createElement('tr');
                tr.innerHTML = `
                    <td>${license.id}</td>
                    <td style="font-weight: 500; color: #fff;">${license.companyName}</td>
                    <td><span style="font-family: monospace; color: #cbd5e1;">${license.hardwareId}</span></td>
                    <td>${license.issueDate}</td>
                    <td>${license.expiredDate}</td>
                    <td>
                        <div style="display: flex; gap: 4px; flex-wrap: wrap;">
                            ${license.modules ? license.modules.split(',').map(m => `<span class="status-badge">${m.trim()}</span>`).join('') : '<span style="color: var(--text-muted)">None</span>'}
                        </div>
                    </td>
                    <td>${license.generatedAt}</td>
                    <td>
                        <div style="max-width: 150px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-family: monospace; color: var(--success);" title="${license.signature}">
                            ${license.signature}
                        </div>
                    </td>
                `;
                tableBody.appendChild(tr);
            });

        } catch (error) {
            tableBody.innerHTML = '<tr><td colspan="8" style="text-align: center; color: var(--error);">Error loading data.</td></tr>';
            errorMessage.textContent = error.message;
            errorToast.classList.remove('hidden');
        }
    }

    loadLicenses();
});
