// ════════════════════════════════════════════════════════════════════════════
//  database.js core
// ════════════════════════════════════════════════════════════════════════════

document.addEventListener('DOMContentLoaded', () => {

    const errorToast = document.getElementById('errorToast');
    const errorMessage = document.getElementById('errorMessage');
    const logoutBtn = document.getElementById('logoutBtn');

    let currentUserRole = '';
    let deleteCallback = null;

    // ─── Data stores (avoid JSON-in-onclick issues) ───────────────────────────
    const companyStore = new Map(); // companyName -> record
    const employeeStore = new Map(); // employeeId -> record

    // ─── ROLE HELPERS ───────────────────────────────────────────────────────────
    const isAdmin = () => currentUserRole === 'admin';
    const isManager = () => ['senior_manager', 'admin'].includes(currentUserRole);
    const isJuniorOrAbove = () => ['junior_manager', 'senior_manager', 'admin'].includes(currentUserRole);


    // ─── LOGOUT ─────────────────────────────────────────────────────────────────
    if (logoutBtn) {
        logoutBtn.addEventListener('click', () => {
            fetch('/api/logout', { method: 'POST' }).then(() => {
                window.location.href = '/login.html';
            });
        });
    }

    // ─── GLOBAL TOAST ────────────────────────────────────────────────────────────
    function showError(msg) {
        errorMessage.textContent = msg;
        errorToast.classList.remove('hidden');
        setTimeout(() => errorToast.classList.add('hidden'), 5000);
    }

    // Безопасно читает тело ответа и возвращает русское сообщение об ошибке
    async function safeDeleteError(response, fallback) {
        try {
            const ct = response.headers.get('content-type') || '';
            if (ct.includes('application/json')) {
                const d = await response.json();
                return d.error || fallback;
            }
        } catch { /* пустое тело или не JSON */ }
        return fallback;
    }

    function showFormError(spanId, boxId, msg) {
        document.getElementById(spanId).textContent = msg;
        document.getElementById(boxId).classList.remove('hidden');
    }

    function hideFormError(boxId) {
        document.getElementById(boxId).classList.add('hidden');
    }

    // ─── INIT / ROLE CHECK ───────────────────────────────────────────────────────
    async function init() {
        try {
            const res = await fetch('/api/me');
            if (!res.ok) { window.location.href = '/login.html'; return; }
            const data = await res.json();
            currentUserRole = data.role;

            fetch('/api/version').then(async resVersion => {
                if (resVersion.ok) {
                    const v = await resVersion.json();
                    const el = document.getElementById('appVersionInfo');
                    if (el) el.textContent = `Версия сервера: ${v.serverVersion} | Версия библиотеки: ${v.libVersion}`;
                }
            });

            // Show/hide tabs
            if (isAdmin()) {
                document.getElementById('tabEmployees').classList.remove('hidden');
                document.getElementById('tabConfigurations').classList.remove('hidden');
            }

            // Show/hide manager-only action columns & buttons
            if (isManager()) {
                document.querySelectorAll('.db-col--manager-only').forEach(el => el.classList.remove('hidden'));
            }
            if (isJuniorOrAbove()) {
                document.querySelectorAll('.db-btn--manager-only').forEach(el => el.classList.remove('hidden'));
            }

        } catch(e) {
            window.location.href = '/login.html';
        }
    }
    init();

    // ─── TAB SWITCHING ───────────────────────────────────────────────────────────
    let licensesLoaded = false;
    let companiesLoaded = false;
    let employeesLoaded = false;
    let configurationsLoaded = false;
    let modulesLoaded = false;
    let allModulesCache = []; // cached modules for config editing

    document.querySelectorAll('.db-tab').forEach(tab => {
        tab.addEventListener('click', () => {
            document.querySelectorAll('.db-tab').forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.add('hidden'));

            tab.classList.add('active');
            const name = tab.dataset.tab;
            document.getElementById(`tabContent${name.charAt(0).toUpperCase() + name.slice(1)}`).classList.remove('hidden');

            if (name === 'licenses' && !licensesLoaded) loadLicenses();
            if (name === 'companies' && !companiesLoaded) loadCompanies();
            if (name === 'employees' && !employeesLoaded) loadEmployees();
            if (name === 'configurations' && !configurationsLoaded) loadConfigurations();
            if (name === 'modules' && !modulesLoaded) loadModules();
        });
    });

    // Load licenses by default
    loadLicenses();

    // ─── CONFIRM DELETE MODAL ────────────────────────────────────────────────────
    const confirmDeleteModal = document.getElementById('confirmDeleteModal');
    document.getElementById('cancelDeleteBtn').addEventListener('click', () => {
        confirmDeleteModal.classList.add('hidden');
        deleteCallback = null;
    });
    document.getElementById('confirmDeleteBtn').addEventListener('click', async () => {
        if (deleteCallback) await deleteCallback();
        confirmDeleteModal.classList.add('hidden');
        deleteCallback = null;
    });
    window.addEventListener('click', e => {
        if (e.target === confirmDeleteModal) {
            confirmDeleteModal.classList.add('hidden');
            deleteCallback = null;
        }
    });

    function showConfirmDelete(msg, cb) {
        document.getElementById('confirmDeleteMsg').textContent = msg;
        deleteCallback = cb;
        confirmDeleteModal.classList.remove('hidden');
    }

    // ─── ROLE BADGE ──────────────────────────────────────────────────────────────
    const ROLE_LABELS = {
        'junior_manager': 'Junior Manager',
        'senior_manager': 'Senior Manager',
        'admin': 'Administrator'
    };
    const ROLE_COLORS = {
        'junior_manager': 'rgba(99,102,241,0.15)',
        'senior_manager': 'rgba(245,158,11,0.15)',
        'admin': 'rgba(139,92,246,0.2)'
    };
    const ROLE_TEXT_COLORS = {
        'junior_manager': '#818cf8',
        'senior_manager': '#fbbf24',
        'admin': '#c4b5fd'
    };

    function roleBadge(role) {
        const bg = ROLE_COLORS[role] || 'rgba(255,255,255,0.05)';
        const color = ROLE_TEXT_COLORS[role] || '#94a3b8';
        const label = ROLE_LABELS[role] || role;
        return `<span style="display:inline-block;padding:0.2rem 0.65rem;border-radius:9999px;font-size:0.75rem;font-weight:600;background:${bg};color:${color};">${label}</span>`;
    }

    // ─── ACTION BUTTONS HTML ──────────────────────────────────────────────────────
    function actionBtns(editFn, deleteFn) {
        return `<div style="display:flex;gap:0.5rem;">
            <button class="action-btn action-btn--edit" onclick="${editFn}">✏️ Редактировать</button>
            <button class="action-btn action-btn--delete" onclick="${deleteFn}">🗑️ Удалить</button>
        </div>`;
    }

    // ════════════════════════════════════════════════════════════════════════════
    //  LICENSES
    // ════════════════════════════════════════════════════════════════════════════
    let allLicensesData = [];
    let currentSort = { key: 'generatedAt', dir: 'desc' };

    function renderLicenses() {
        const tbody = document.getElementById('licensesTableBody');
        tbody.innerHTML = '';

        const fCompany = document.getElementById('filterCompany')?.value.toLowerCase() || '';
        const fIssueDate = document.getElementById('filterIssueDate')?.value.toLowerCase() || '';
        const fExpiredDate = document.getElementById('filterExpiredDate')?.value.toLowerCase() || '';
        const fGeneratedAt = document.getElementById('filterGeneratedAt')?.value.toLowerCase() || '';

        let filtered = allLicensesData.filter(lic => {
            if (fCompany && (!lic.companyName || !lic.companyName.toLowerCase().includes(fCompany))) return false;
            if (fIssueDate && (!lic.issueDate || !lic.issueDate.toLowerCase().includes(fIssueDate))) return false;
            if (fExpiredDate && (!lic.expiredDate || !lic.expiredDate.toLowerCase().includes(fExpiredDate))) return false;
            if (fGeneratedAt && (!lic.generatedAt || !lic.generatedAt.toLowerCase().includes(fGeneratedAt))) return false;
            return true;
        });

        filtered.sort((a, b) => {
            let valA = a[currentSort.key] || '';
            let valB = b[currentSort.key] || '';
            if (valA < valB) return currentSort.dir === 'asc' ? -1 : 1;
            if (valA > valB) return currentSort.dir === 'asc' ? 1 : -1;
            return 0;
        });

        if (filtered.length === 0) {
            tbody.innerHTML = '<tr><td colspan="9" style="text-align:center;padding:2rem;color:var(--text-muted);">Лицензии не найдены.</td></tr>';
            return;
        }

        filtered.forEach((lic, index) => {
            const tr = document.createElement('tr');
            const modulesBadges = lic.modules
                ? lic.modules.split(',').map(m => `<span class="status-badge">${m.trim()}</span>`).join('')
                : '<span style="color:var(--text-muted)">Нет</span>';

            const actionsHtml = `<td><div style="display:flex;gap:0.5rem;align-items:center;">
                <button class="action-btn" onclick="window.__downloadLicense('${lic.id}')" style="background:var(--primary);color:#fff;border-color:transparent;" title="Скачать JSON">💾</button>
                ${isAdmin() ? `<button class="action-btn action-btn--hwid" onclick="window.__downloadHwId('${lic.id}')" title="Скачать Hardware ID (.bin)">💻</button>` : ''}
                ${isManager() ? `<button class="action-btn action-btn--delete" onclick="window.__deleteLicense('${lic.id}')">🗑️</button>` : ''}
            </div></td>`;

            tr.innerHTML = `
                <td style="font-weight:500;color:#c4b5fd;">${index + 1}</td>
                <td style="font-weight:500;color:#fff;">${esc(lic.companyName)}</td>
                <td><span style="font-family:monospace;color:#cbd5e1;">${esc(lic.hardwareId)}</span></td>
                <td>${esc(lic.issueDate)}</td>
                <td>${esc(lic.expiredDate)}</td>
                <td><div style="display:flex;gap:4px;flex-wrap:wrap;">${modulesBadges}</div></td>
                <td style="color:var(--text-muted);font-size:0.85rem;">${esc(lic.generatedAt)}</td>
                <td><div style="font-family:monospace;font-size:0.8rem;color:var(--success);word-break:break-all;max-width:300px;">${esc(lic.signature)}</div></td>
                ${actionsHtml}
            `;
            tbody.appendChild(tr);
        });
    }

    document.querySelectorAll('.filter-input').forEach(input => {
        input.addEventListener('input', renderLicenses);
    });
    document.querySelectorAll('.sort-icon').forEach(icon => {
        icon.addEventListener('click', (e) => {
            const key = e.target.dataset.sort;
            if (currentSort.key === key) {
                currentSort.dir = currentSort.dir === 'asc' ? 'desc' : 'asc';
            } else {
                currentSort.key = key;
                currentSort.dir = 'asc';
            }
            renderLicenses();
        });
    });

    async function loadLicenses() {
        const tbody = document.getElementById('licensesTableBody');
        tbody.innerHTML = '<tr><td colspan="9" style="text-align:center;padding:2rem;"><div class="loader inline-loader" style="margin:0 auto;border-top-color:var(--primary);"></div></td></tr>';

        try {
            const res = await fetch('/api/licenses');
            if (!res.ok) { if (res.status === 401) { window.location.href = '/login.html'; return; } throw new Error('Не удалось загрузить список лицензий.'); }
            allLicensesData = await res.json();
            licensesLoaded = true;
            renderLicenses();

            window.__deleteLicense = (id) => {
                showConfirmDelete(`Удалить лицензию? Это действие нельзя будет отменить.`, async () => {
                    try {
                        const r = await fetch(`/api/licenses/${id}`, { method: 'DELETE' });
                        if (!r.ok) { throw new Error(await safeDeleteError(r, 'Не удалось удалить лицензию.')); }
                        licensesLoaded = false;
                        await loadLicenses();
                    } catch(e) { showError(e.message); }
                });
            };

            window.__downloadLicense = (id) => {
                const lic = allLicensesData.find(l => String(l.id) === String(id));
                if (!lic) return;
                const dataObj = {
                    licenseDesc: {
                        company: lic.companyName,
                        expiredDate: lic.expiredDate,
                        issueDate: lic.issueDate,
                        modules: lic.modules ? lic.modules.split(',').map(m => m.trim()) : [],
                        signature: lic.signature
                    }
                };
                const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(dataObj, null, 4));
                const anchor = document.createElement('a');
                anchor.setAttribute("href", dataStr);
                anchor.setAttribute("download", "license_" + lic.companyName.replace(/[^a-z0-9а-яё]+/gi, '_').toLowerCase() + ".json");
                document.body.appendChild(anchor);
                anchor.click();
                anchor.remove();
            };

            window.__downloadHwId = (id) => {
                const lic = allLicensesData.find(l => String(l.id) === String(id));
                if (!lic || !lic.hardwareId) return;

                // Convert hardware ID string to binary format
                // Assuming hardwareId is a hex string (e.g., "A1B2C3D4") or alphanumeric
                // If it's hex, convert to bytes; otherwise use UTF-8 encoding
                let bytes;
                const hwid = lic.hardwareId.trim();

                // Check if it's a valid hex string (only 0-9, A-F, a-f, optional dashes)
                if (/^[0-9A-Fa-f\-]+$/.test(hwid)) {
                    // Remove dashes and convert hex to bytes
                    const hexStr = hwid.replace(/-/g, '');
                    bytes = new Uint8Array(hexStr.length / 2);
                    for (let i = 0; i < hexStr.length; i += 2) {
                        bytes[i / 2] = parseInt(hexStr.substr(i, 2), 16);
                    }
                } else {
                    // Not hex, use UTF-8 encoding
                    bytes = new TextEncoder().encode(hwid);
                }

                const blob = new Blob([bytes], { type: 'application/octet-stream' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = 'hardware_id_' + lic.companyName.replace(/[^a-z0-9а-яё]+/gi, '_').toLowerCase() + '.bin';
                document.body.appendChild(a);
                a.click();
                a.remove();
                URL.revokeObjectURL(url);
            };

        } catch(err) {
            tbody.innerHTML = '<tr><td colspan="9" style="text-align:center;padding:2rem;color:var(--error);">Ошибка при загрузке лицензий.</td></tr>';
            showError(err.message);
        }
    }

    // ════════════════════════════════════════════════════════════════════════════
    //  COMPANIES
    // ════════════════════════════════════════════════════════════════════════════
    async function loadCompanies() {
        const tbody = document.getElementById('companiesTableBody');
        tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;padding:2rem;"><div class="loader inline-loader" style="margin:0 auto;border-top-color:var(--primary);"></div></td></tr>';

        try {
            const res = await fetch('/api/companies');
            if (!res.ok) throw new Error('Не удалось загрузить список компаний.');
            const data = await res.json();
            companiesLoaded = true;
            tbody.innerHTML = '';

            if (data.length === 0) {
                tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;padding:2rem;color:var(--text-muted);">Компании не найдены.</td></tr>';
                return;
            }

            data.forEach(c => {
                const tr = document.createElement('tr');
                companyStore.set(c.companyName, c); // store for later lookup

                let contacts = '';
                try {
                    const parsed = typeof c.contacts === 'string' ? JSON.parse(c.contacts) : c.contacts;
                    if (Array.isArray(parsed)) {
                        contacts = parsed.filter(e => e.type || e.value)
                            .map(e => `<span style="color:var(--text-muted);font-size:0.8rem;">${esc(e.type)}:</span> ${esc(e.value)}`).join('<br>');
                    } else if (parsed && typeof parsed === 'object') {
                        contacts = Object.entries(parsed).map(([k, v]) => `<span style="color:var(--text-muted);font-size:0.8rem;">${esc(k)}:</span> ${esc(v)}`).join('<br>');
                    }
                } catch { contacts = esc(c.contacts || ''); }

                const actionsHtml = isManager()
                    ? `<td><div style="display:flex;gap:0.5rem;">
                        <button class="action-btn action-btn--edit" data-action="edit-company" data-key="${esc(c.companyName)}">✏️ Редактировать</button>
                        <button class="action-btn action-btn--delete" data-action="delete-company" data-key="${esc(c.companyName)}">🗑️ Удалить</button>
                    </div></td>`
                    : '<td class="db-col--manager-only hidden"></td>';

                tr.innerHTML = `
                    <td style="font-weight:600;color:#fff;">${esc(c.companyName)}</td>
                    <td style="color:var(--text-muted);">${esc(c.city || '—')}</td>
                    <td>${contacts || '<span style="color:var(--text-muted)">—</span>'}</td>
                    <td style="color:var(--text-muted);font-size:0.85rem;">${esc(c.dateAdded)}</td>
                    ${actionsHtml}
                `;
                tbody.appendChild(tr);
            });

            // Event delegation for company actions
            tbody.addEventListener('click', (e) => {
                const btn = e.target.closest('[data-action]');
                if (!btn) return;
                const key = btn.dataset.key;
                const action = btn.dataset.action;

                if (action === 'edit-company') {
                    const c = companyStore.get(key);
                    if (!c) return;
                    document.getElementById('editCompanyOriginalName').value = c.companyName;
                    document.getElementById('editCompanyName').value = c.companyName;
                    document.getElementById('editCompanyCity').value = c.city || '';
                    window.populateContactsEditor('editCompanyContactsEditor', c.contacts);
                    hideFormError('editCompanyError');
                    document.getElementById('editCompanyModal').classList.remove('hidden');
                }

                if (action === 'delete-company') {
                    showConfirmDelete(`Удалить компанию "${key}"? Связанные лицензии также могут быть удалены.`, async () => {
                        try {
                            const r = await fetch(`/api/companies/${encodeURIComponent(key)}`, { method: 'DELETE' });
                            if (!r.ok) { throw new Error(await safeDeleteError(r, 'Не удалось удалить компанию.')); }
                            companiesLoaded = false;
                            loadCompanies();
                        } catch(e2) { showError(e2.message); }
                    });
                }
            }, /* persistent listener */); // no { once: true } — must work for all rows

        } catch(err) {
            tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;padding:2rem;color:var(--error);">Ошибка при загрузке компаний.</td></tr>';
            showError(err.message);
        }
    }

    // Edit company form
    const editCompanyModal = document.getElementById('editCompanyModal');
    document.getElementById('closeEditCompanyModal').addEventListener('click', () => editCompanyModal.classList.add('hidden'));
    window.addEventListener('click', e => { if (e.target === editCompanyModal) editCompanyModal.classList.add('hidden'); });

    document.getElementById('editCompanyForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        hideFormError('editCompanyError');
        const btn = document.getElementById('editCompanySubmitBtn');
        btn.disabled = true; btn.textContent = 'Сохранение...';

        const originalName = document.getElementById('editCompanyOriginalName').value;
        const data = {
            companyName: document.getElementById('editCompanyName').value,
            city: document.getElementById('editCompanyCity').value,
            contacts: window.readContactsEditor('editCompanyContactsEditor')
        };

        try {
            const r = await fetch(`/api/companies/${encodeURIComponent(originalName)}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });
            if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Ошибка обновления компании.'); }
            editCompanyModal.classList.add('hidden');
            companiesLoaded = false;
            loadCompanies();
        } catch(err) {
            showFormError('editCompanyErrorMsg', 'editCompanyError', err.message);
        } finally {
            btn.disabled = false; btn.textContent = 'Сохранить изменения';
        }
    });

    // Add company from DB page
    const addCompanyDbModal = document.getElementById('addCompanyDbModal');
    document.getElementById('addCompanyDbBtn').addEventListener('click', () => {
        document.getElementById('addDbCompanyName').value = '';
        document.getElementById('addDbCompanyCity').value = '';
        window.populateContactsEditor('addDbCompanyContactsEditor', null);
        hideFormError('addDbCompanyError');
        addCompanyDbModal.classList.remove('hidden');
    });
    document.getElementById('closeAddCompanyDbModal').addEventListener('click', () => addCompanyDbModal.classList.add('hidden'));
    window.addEventListener('click', e => { if (e.target === addCompanyDbModal) addCompanyDbModal.classList.add('hidden'); });

    document.getElementById('addCompanyDbForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        hideFormError('addDbCompanyError');
        const btn = document.getElementById('addDbCompanySubmitBtn');
        btn.disabled = true; btn.textContent = 'Добавление...';

        const data = {
            companyName: document.getElementById('addDbCompanyName').value,
            city: document.getElementById('addDbCompanyCity').value,
            contacts: window.readContactsEditor('addDbCompanyContactsEditor')
        };

        try {
            const r = await fetch('/api/companies', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });
            if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Ошибка при добавлении компании.'); }
            addCompanyDbModal.classList.add('hidden');
            companiesLoaded = false;
            loadCompanies();
        } catch(err) {
            showFormError('addDbCompanyErrorMsg', 'addDbCompanyError', err.message);
        } finally {
            btn.disabled = false; btn.textContent = 'Добавить компанию';
        }
    });

    // ════════════════════════════════════════════════════════════════════════════
    //  EMPLOYEES
    // ════════════════════════════════════════════════════════════════════════════
    async function loadEmployees() {
        const tbody = document.getElementById('employeesTableBody');
        tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;padding:2rem;"><div class="loader inline-loader" style="margin:0 auto;border-top-color:var(--primary);"></div></td></tr>';

        try {
            const res = await fetch('/api/employees');
            if (!res.ok) {
                if (res.status === 403) { tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;padding:2rem;color:var(--error);">Доступ запрещён.</td></tr>'; return; }
                throw new Error('Не удалось загрузить список сотрудников.');
            }
            const data = await res.json();
            employeesLoaded = true;
            tbody.innerHTML = '';

            if (data.length === 0) {
                tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;padding:2rem;color:var(--text-muted);">Сотрудники не найдены.</td></tr>';
                return;
            }

            data.forEach((emp, index) => {
                const tr = document.createElement('tr');
                employeeStore.set(emp.employeeId, emp); // store for later lookup

                tr.innerHTML = `
                    <td style="font-family:monospace;color:#c4b5fd;">${index + 1}</td>
                    <td style="font-weight:500;">${esc(emp.login)}</td>
                    <td>${roleBadge(emp.role)}</td>
                    <td>${esc(emp.lastName)}</td>
                    <td>${esc(emp.firstName)}</td>
                    <td style="color:var(--text-muted);">${esc(emp.middleName || '—')}</td>
                    <td><div style="display:flex;gap:0.5rem;">
                        <button class="action-btn action-btn--edit" data-action="edit-employee" data-key="${esc(emp.employeeId)}">✏️ Редактировать</button>
                        <button class="action-btn action-btn--delete" data-action="delete-employee" data-key="${esc(emp.employeeId)}" data-login="${esc(emp.login)}">🗑️ Удалить</button>
                    </div></td>
                `;
                tbody.appendChild(tr);
            });

            // Event delegation for employee actions
            tbody.addEventListener('click', (e) => {
                const btn = e.target.closest('[data-action]');
                if (!btn) return;
                const key = btn.dataset.key;
                const action = btn.dataset.action;

                if (action === 'edit-employee') {
                    const emp = employeeStore.get(key);
                    if (!emp) return;
                    document.getElementById('editEmployeeId').value = emp.employeeId;
                    document.getElementById('editEmpLogin').value = emp.login;
                    document.getElementById('editEmpPassword').value = '';
                    document.getElementById('editEmpRole').value = emp.role;
                    document.getElementById('editEmpFirstName').value = emp.firstName;
                    document.getElementById('editEmpLastName').value = emp.lastName;
                    document.getElementById('editEmpMiddleName').value = emp.middleName || '';
                    document.getElementById('editEmployeeTitle').textContent = `Редактировать сотрудника: ${emp.login}`;
                    hideFormError('editEmployeeError');
                    document.getElementById('editEmployeeModal').classList.remove('hidden');
                }

                if (action === 'delete-employee') {
                    const login = btn.dataset.login;
                    showConfirmDelete(`Удалить сотрудника "${login}" (ID: ${key})? Его аккаунт также будет удален.`, async () => {
                        try {
                            const r = await fetch(`/api/employees/${encodeURIComponent(key)}`, { method: 'DELETE' });
                            if (!r.ok) { throw new Error(await safeDeleteError(r, 'Не удалось удалить сотрудника.')); }
                            employeesLoaded = false;
                            loadEmployees();
                        } catch(e2) { showError(e2.message); }
                    });
                }
            }, /* persistent listener */); // no { once: true } — must work for all rows

        } catch(err) {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;padding:2rem;color:var(--error);">Ошибка при загрузке сотрудников.</td></tr>';
            showError(err.message);
        }
    }

    // Edit/Add employee modal
    const editEmployeeModal = document.getElementById('editEmployeeModal');
    document.getElementById('closeEditEmployeeModal').addEventListener('click', () => editEmployeeModal.classList.add('hidden'));
    window.addEventListener('click', e => { if (e.target === editEmployeeModal) editEmployeeModal.classList.add('hidden'); });

    // Add Employee from DB tab
    document.getElementById('addEmployeeDbBtn').addEventListener('click', () => {
        document.getElementById('editEmployeeId').value = '';
        document.getElementById('editEmployeeForm').reset();
        document.getElementById('editEmployeeTitle').textContent = 'Добавить нового сотрудника';
        document.getElementById('editEmployeeSubmitBtn').textContent = 'Создать сотрудника';
        hideFormError('editEmployeeError');
        editEmployeeModal.classList.remove('hidden');
    });

    document.getElementById('editEmployeeForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        hideFormError('editEmployeeError');
        const btn = document.getElementById('editEmployeeSubmitBtn');
        btn.disabled = true; btn.textContent = 'Сохранение...';

        const empId = document.getElementById('editEmployeeId').value;
        const isNew = !empId;

        const data = {
            login: document.getElementById('editEmpLogin').value,
            role: document.getElementById('editEmpRole').value,
            first_name: document.getElementById('editEmpFirstName').value,
            last_name: document.getElementById('editEmpLastName').value,
            middle_name: document.getElementById('editEmpMiddleName').value
        };
        const pwd = document.getElementById('editEmpPassword').value;
        if (pwd || isNew) data.password = pwd;

        try {
            let r;
            if (isNew) {
                r = await fetch('/api/employees', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
            } else {
                r = await fetch(`/api/employees/${encodeURIComponent(empId)}`, {
                    method: 'PUT',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
            }
            if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Ошибка сохранения данных сотрудника.'); }
            editEmployeeModal.classList.add('hidden');
            employeesLoaded = false;
            loadEmployees();
        } catch(err) {
            showFormError('editEmployeeErrorMsg', 'editEmployeeError', err.message);
        } finally {
            btn.disabled = false;
            btn.textContent = empId ? 'Сохранить сотрудника' : 'Создать сотрудника';
        }
    });

    // ════════════════════════════════════════════════════════════════════════════
    //  CONFIGURATIONS
    // ════════════════════════════════════════════════════════════════════════════
    const configStore = new Map();

    async function fetchAllModules() {
        if (allModulesCache.length) return allModulesCache;
        try {
            const res = await fetch('/api/modules');
            if (res.ok) allModulesCache = await res.json();
        } catch(e) { console.error('fetchAllModules', e); }
        return allModulesCache;
    }

    function renderModuleCheckboxes(containerId, selectedModules = []) {
        const container = document.getElementById(containerId);
        if (!container) return;
        container.innerHTML = '';
        const selSet = new Set(selectedModules);
        for (const mod of allModulesCache) {
            const label = document.createElement('label');
            label.className = 'module-check-item';
            const cb = document.createElement('input');
            cb.type = 'checkbox';
            cb.value = mod.moduleName;
            cb.checked = selSet.has(mod.moduleName);
            const span = document.createElement('span');
            span.textContent = (mod.displayLabel || mod.moduleName) + (mod.parentModule ? ` (→ ${mod.parentModule})` : ' [группа]');
            label.appendChild(cb);
            label.appendChild(span);
            container.appendChild(label);
        }
        if (!allModulesCache.length) {
            container.innerHTML = '<span style="color:var(--text-muted);font-size:0.85rem;">Модули не найдены.</span>';
        }
    }

    function readModuleCheckboxes(containerId) {
        const container = document.getElementById(containerId);
        if (!container) return [];
        return [...container.querySelectorAll('input[type=checkbox]:checked')].map(cb => cb.value);
    }

    async function loadConfigurations() {
        const tbody = document.getElementById('configurationsTableBody');
        tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;padding:2rem;"><div class="loader inline-loader" style="margin:0 auto;border-top-color:var(--primary);"></div></td></tr>';

        try {
            await fetchAllModules();
            const res = await fetch('/api/configurations');
            if (!res.ok) throw new Error('Не удалось загрузить конфигурации.');
            const data = await res.json();
            configurationsLoaded = true;
            configStore.clear();
            tbody.innerHTML = '';

            if (data.length === 0) {
                tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;padding:2rem;color:var(--text-muted);">Конфигурации не найдены.</td></tr>';
                return;
            }

            data.forEach((cfg, idx) => {
                configStore.set(String(cfg.configId), cfg);
                const tr = document.createElement('tr');
                const modulesBadges = cfg.modules && cfg.modules.length
                    ? cfg.modules.map(m => `<span class="status-badge">${esc(m)}</span>`).join('')
                    : '<span style="color:var(--text-muted)">—</span>';

                tr.innerHTML = `
                    <td style="font-weight:500;color:#c4b5fd;">${idx + 1}</td>
                    <td style="font-weight:600;color:#fff;">${esc(cfg.configName)}</td>
                    <td style="color:var(--text-muted);">${esc(cfg.description || '—')}</td>
                    <td><div class="config-modules-cell">${modulesBadges}</div></td>
                    <td><div style="display:flex;gap:0.5rem;">
                        <button class="action-btn action-btn--edit" data-action="edit-config" data-key="${cfg.configId}">✏️ Редактировать</button>
                        <button class="action-btn action-btn--delete" data-action="delete-config" data-key="${cfg.configId}" data-name="${esc(cfg.configName)}">🗑️ Удалить</button>
                    </div></td>
                `;
                tbody.appendChild(tr);
            });

            tbody.addEventListener('click', (e) => {
                const btn = e.target.closest('[data-action]');
                if (!btn) return;
                const key = btn.dataset.key;
                const action = btn.dataset.action;

                if (action === 'edit-config') {
                    const cfg = configStore.get(key);
                    if (!cfg) return;
                    document.getElementById('editConfigId').value = cfg.configId;
                    document.getElementById('editConfigName').value = cfg.configName;
                    document.getElementById('editConfigDesc').value = cfg.description || '';
                    renderModuleCheckboxes('editConfigModuleList', cfg.modules || []);
                    hideFormError('editConfigError');
                    document.getElementById('editConfigModal').classList.remove('hidden');
                }

                if (action === 'delete-config') {
                    const name = btn.dataset.name;
                    showConfirmDelete(`Удалить конфигурацию "${name}"?`, async () => {
                        try {
                            const r = await fetch(`/api/configurations/${key}`, { method: 'DELETE' });
                            if (!r.ok) throw new Error(await safeDeleteError(r, 'Не удалось удалить конфигурацию.'));
                            configurationsLoaded = false;
                            loadConfigurations();
                        } catch(e2) { showError(e2.message); }
                    });
                }
            });

        } catch(err) {
            tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;padding:2rem;color:var(--error);">Ошибка при загрузке конфигураций.</td></tr>';
            showError(err.message);
        }
    }

    // Add configuration modal
    const addConfigModal = document.getElementById('addConfigModal');
    if (addConfigModal) {
        document.getElementById('addConfigDbBtn').addEventListener('click', async () => {
            document.getElementById('addConfigName').value = '';
            document.getElementById('addConfigDesc').value = '';
            await fetchAllModules();
            renderModuleCheckboxes('addConfigModuleList', []);
            hideFormError('addConfigError');
            addConfigModal.classList.remove('hidden');
        });
        document.getElementById('closeAddConfigModal').addEventListener('click', () => addConfigModal.classList.add('hidden'));
        window.addEventListener('click', e => { if (e.target === addConfigModal) addConfigModal.classList.add('hidden'); });

        document.getElementById('addConfigForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            hideFormError('addConfigError');
            const btn = document.getElementById('addConfigSubmitBtn');
            btn.disabled = true; btn.textContent = 'Добавление...';

            const data = {
                configName: document.getElementById('addConfigName').value.trim(),
                description: document.getElementById('addConfigDesc').value.trim(),
                modules: readModuleCheckboxes('addConfigModuleList')
            };

            try {
                const r = await fetch('/api/configurations', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
                if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Ошибка добавления конфигурации.'); }
                addConfigModal.classList.add('hidden');
                configurationsLoaded = false;
                loadConfigurations();
            } catch(err) {
                showFormError('addConfigErrorMsg', 'addConfigError', err.message);
            } finally {
                btn.disabled = false; btn.textContent = 'Добавить конфигурацию';
            }
        });
    }

    // Edit configuration modal
    const editConfigModal = document.getElementById('editConfigModal');
    if (editConfigModal) {
        document.getElementById('closeEditConfigModal').addEventListener('click', () => editConfigModal.classList.add('hidden'));
        window.addEventListener('click', e => { if (e.target === editConfigModal) editConfigModal.classList.add('hidden'); });

        document.getElementById('editConfigForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            hideFormError('editConfigError');
            const btn = document.getElementById('editConfigSubmitBtn');
            btn.disabled = true; btn.textContent = 'Сохранение...';

            const configId = document.getElementById('editConfigId').value;
            const data = {
                configName: document.getElementById('editConfigName').value.trim(),
                description: document.getElementById('editConfigDesc').value.trim(),
                modules: readModuleCheckboxes('editConfigModuleList')
            };

            try {
                const r = await fetch(`/api/configurations/${configId}`, {
                    method: 'PUT',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
                if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Ошибка обновления конфигурации.'); }
                editConfigModal.classList.add('hidden');
                configurationsLoaded = false;
                loadConfigurations();
            } catch(err) {
                showFormError('editConfigErrorMsg', 'editConfigError', err.message);
            } finally {
                btn.disabled = false; btn.textContent = 'Сохранить изменения';
            }
        });
    }

    // ════════════════════════════════════════════════════════════════════════════
    //  MODULES
    // ════════════════════════════════════════════════════════════════════════════
    const moduleStore = new Map();
    let allModulesData = [];

    async function loadModules() {
        const tbody = document.getElementById('modulesTableBody');
        tbody.innerHTML = '<tr><td colspan="8" style="text-align:center;padding:2rem;"><div class="loader inline-loader" style="margin:0 auto;border-top-color:var(--primary);"></div></td></tr>';

        try {
            const res = await fetch('/api/modules');
            if (!res.ok) throw new Error('Не удалось загрузить список модулей.');
            const data = await res.json();
            allModulesData = data;
            modulesLoaded = true;
            tbody.innerHTML = '';

            if (data.length === 0) {
                tbody.innerHTML = '<tr><td colspan="8" style="text-align:center;padding:2rem;color:var(--text-muted);">Модули не найдены.</td></tr>';
                return;
            }

            // Build tree structure
            const rootModules = data.filter(m => !m.parentModule);
            const childMap = new Map();
            data.forEach(m => {
                if (m.parentModule) {
                    if (!childMap.has(m.parentModule)) childMap.set(m.parentModule, []);
                    childMap.get(m.parentModule).push(m);
                }
                moduleStore.set(m.moduleName, m);
            });

            function renderModule(mod, level = 0) {
                const tr = document.createElement('tr');
                const indent = '&nbsp;'.repeat(level * 4);
                const icon = level > 0 ? '└─ ' : '';

                tr.innerHTML = `
                    <td style="font-family:monospace;color:#c4b5fd;">${indent}${icon}${esc(mod.moduleName)}</td>
                    <td>${esc(mod.displayLabel)}</td>
                    <td style="color:var(--text-muted);">${mod.parentModule ? esc(mod.parentModule) : '—'}</td>
                    <td style="text-align:center;">${mod.sortOrder}</td>
                    <td style="text-align:center;">${mod.isSelectable ? '<span style="color:var(--success);">✓</span>' : '<span style="color:var(--text-muted);">✗</span>'}</td>
                    <td style="text-align:center;">${mod.requiresDevice ? '<span style="color:var(--warning);">✓</span>' : '<span style="color:var(--text-muted);">✗</span>'}</td>
                    <td style="color:var(--text-muted);font-size:0.85rem;">${mod.dependencyGroup || '—'}</td>
                    <td>
                        <div style="display:flex;gap:0.5rem;">
                            <button class="action-btn" onclick="window.__editModule('${mod.moduleName}')" style="background:var(--primary);color:#fff;" title="Редактировать">✏️</button>
                            <button class="action-btn action-btn--delete" onclick="window.__deleteModule('${mod.moduleName}')" title="Удалить">🗑️</button>
                        </div>
                    </td>
                `;
                tbody.appendChild(tr);

                // Render children
                const children = childMap.get(mod.moduleName) || [];
                children.sort((a, b) => a.sortOrder - b.sortOrder);
                children.forEach(child => renderModule(child, level + 1));
            }

            rootModules.sort((a, b) => a.sortOrder - b.sortOrder);
            rootModules.forEach(mod => renderModule(mod));

            // Global functions for actions
            window.__editModule = (name) => {
                const mod = moduleStore.get(name);
                if (!mod) return;
                document.getElementById('editModuleName').value = mod.moduleName;
                document.getElementById('editModuleNameDisplay').value = mod.moduleName;
                document.getElementById('editModuleDisplayLabel').value = mod.displayLabel;
                document.getElementById('editModuleDescription').value = mod.description || '';
                document.getElementById('editModuleSortOrder').value = mod.sortOrder;
                document.getElementById('editModuleSelectable').checked = mod.isSelectable;
                document.getElementById('editModuleRequiresDevice').checked = mod.requiresDevice;
                document.getElementById('editModuleDependencyGroup').value = mod.dependencyGroup || '';
                document.getElementById('editModuleRequiredWith').value = (mod.requiredWith || []).join(', ');

                // Populate parent dropdown
                const parentSelect = document.getElementById('editModuleParent');
                parentSelect.innerHTML = '<option value="">-- Корневой модуль --</option>';
                allModulesData.filter(m => m.moduleName !== mod.moduleName).forEach(m => {
                    const opt = document.createElement('option');
                    opt.value = m.moduleName;
                    opt.textContent = m.displayLabel || m.moduleName;
                    if (m.moduleName === mod.parentModule) opt.selected = true;
                    parentSelect.appendChild(opt);
                });

                document.getElementById('editModuleModal').classList.remove('hidden');
            };

            window.__deleteModule = (name) => {
                deleteCallback = async () => {
                    try {
                        const r = await fetch(`/api/modules/${encodeURIComponent(name)}`, { method: 'DELETE' });
                        if (!r.ok) {
                            const msg = await safeDeleteError(r, 'Не удалось удалить модуль.');
                            showError(msg);
                            return;
                        }
                        modulesLoaded = false;
                        loadModules();
                    } catch(err) {
                        showError(err.message);
                    }
                };
                document.getElementById('confirmDeleteModal').classList.remove('hidden');
            };

        } catch(err) {
            tbody.innerHTML = '<tr><td colspan="8" style="text-align:center;padding:2rem;color:var(--error);">Ошибка при загрузке модулей.</td></tr>';
            showError(err.message);
        }
    }

    // Add module modal
    const addModuleModal = document.getElementById('addModuleModal');
    if (addModuleModal) {
        document.getElementById('addModuleBtn').addEventListener('click', async () => {
            // Populate parent dropdown
            await fetchAllModules();
            const parentSelect = document.getElementById('addModuleParent');
            parentSelect.innerHTML = '<option value="">-- Корневой модуль --</option>';
            allModulesCache.forEach(m => {
                const opt = document.createElement('option');
                opt.value = m.moduleName;
                opt.textContent = m.displayLabel || m.moduleName;
                parentSelect.appendChild(opt);
            });
            addModuleModal.classList.remove('hidden');
        });

        document.getElementById('closeAddModuleModal').addEventListener('click', () => addModuleModal.classList.add('hidden'));
        window.addEventListener('click', e => { if (e.target === addModuleModal) addModuleModal.classList.add('hidden'); });

        document.getElementById('addModuleForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            hideFormError('addModuleError');
            const btn = document.getElementById('addModuleSubmitBtn');
            btn.disabled = true; btn.textContent = 'Добавление...';

            const requiredWithStr = document.getElementById('addModuleRequiredWith').value.trim();
            const requiredWith = requiredWithStr ? requiredWithStr.split(',').map(s => s.trim()).filter(s => s) : [];

            const data = {
                moduleName: document.getElementById('addModuleName').value.trim(),
                displayLabel: document.getElementById('addModuleDisplayLabel').value.trim(),
                description: document.getElementById('addModuleDescription').value.trim(),
                parentModule: document.getElementById('addModuleParent').value,
                sortOrder: parseInt(document.getElementById('addModuleSortOrder').value) || 0,
                isSelectable: document.getElementById('addModuleSelectable').checked,
                requiresDevice: document.getElementById('addModuleRequiresDevice').checked,
                dependencyGroup: document.getElementById('addModuleDependencyGroup').value.trim(),
                requiredWith: requiredWith
            };

            try {
                const r = await fetch('/api/modules', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
                if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Ошибка добавления модуля.'); }
                addModuleModal.classList.add('hidden');
                document.getElementById('addModuleForm').reset();
                allModulesCache = []; // Clear cache
                modulesLoaded = false;
                loadModules();
            } catch(err) {
                showFormError('addModuleErrorMsg', 'addModuleError', err.message);
            } finally {
                btn.disabled = false; btn.textContent = 'Добавить модуль';
            }
        });
    }

    // Edit module modal
    const editModuleModal = document.getElementById('editModuleModal');
    if (editModuleModal) {
        document.getElementById('closeEditModuleModal').addEventListener('click', () => editModuleModal.classList.add('hidden'));
        window.addEventListener('click', e => { if (e.target === editModuleModal) editModuleModal.classList.add('hidden'); });

        document.getElementById('editModuleForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            hideFormError('editModuleError');
            const btn = document.getElementById('editModuleSubmitBtn');
            btn.disabled = true; btn.textContent = 'Сохранение...';

            const moduleName = document.getElementById('editModuleName').value;
            const requiredWithStr = document.getElementById('editModuleRequiredWith').value.trim();
            const requiredWith = requiredWithStr ? requiredWithStr.split(',').map(s => s.trim()).filter(s => s) : [];

            const data = {
                displayLabel: document.getElementById('editModuleDisplayLabel').value.trim(),
                description: document.getElementById('editModuleDescription').value.trim(),
                parentModule: document.getElementById('editModuleParent').value,
                sortOrder: parseInt(document.getElementById('editModuleSortOrder').value) || 0,
                isSelectable: document.getElementById('editModuleSelectable').checked,
                requiresDevice: document.getElementById('editModuleRequiresDevice').checked,
                dependencyGroup: document.getElementById('editModuleDependencyGroup').value.trim(),
                requiredWith: requiredWith,
                isActive: true
            };

            try {
                const r = await fetch(`/api/modules/${encodeURIComponent(moduleName)}`, {
                    method: 'PUT',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
                if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Ошибка обновления модуля.'); }
                editModuleModal.classList.add('hidden');
                allModulesCache = []; // Clear cache
                modulesLoaded = false;
                loadModules();
            } catch(err) {
                showFormError('editModuleErrorMsg', 'editModuleError', err.message);
            } finally {
                btn.disabled = false; btn.textContent = 'Сохранить изменения';
            }
        });
    }

    // ─── UTILITY ─────────────────────────────────────────────────────────────────
    function esc(str) {
        if (str == null) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#039;');
    }
});
