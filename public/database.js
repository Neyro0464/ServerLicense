// ════════════════════════════════════════════════════════════════════════════
//  SHARED CONTACTS EDITOR HELPERS (used by database.js and app.js)
// ════════════════════════════════════════════════════════════════════════════

/**
 * Add one empty type+value row to a contacts editor container.
 * @param {string} containerId - the id of the .contacts-editor div
 * @param {string} type - prefill the "type" field (optional)
 * @param {string} value - prefill the "value" field (optional)
 */
window.addContactRow = function(containerId, type = '', value = '') {
    const container = document.getElementById(containerId);
    if (!container) return;

    const row = document.createElement('div');
    row.className = 'contact-row';
    row.innerHTML = `
        <input type="text" class="contact-type" placeholder="Contact type (e.g. Phone)" value="${escAttr(type)}" autocomplete="off">
        <input type="text" class="contact-value" placeholder="Value (e.g. +7 999 123-45-67)" value="${escAttr(value)}" autocomplete="off">
        <button type="button" class="contact-remove-btn" title="Remove" onclick="this.closest('.contact-row').remove()">✕</button>
    `;
    container.appendChild(row);
};

/**
 * Populate a contacts editor from an existing contacts JSON string or object.
 * Handles both array format [{type,value}] and legacy object format {key:value}.
 * Clears the container first, then adds one empty row if no contacts exist.
 */
window.populateContactsEditor = function(containerId, contactsRaw) {
    const container = document.getElementById(containerId);
    if (!container) return;
    container.innerHTML = '';

    let entries = [];
    try {
        const parsed = typeof contactsRaw === 'string' ? JSON.parse(contactsRaw) : contactsRaw;
        if (Array.isArray(parsed)) {
            entries = parsed; // [{type, value}]
        } else if (parsed && typeof parsed === 'object') {
            entries = Object.entries(parsed).map(([k, v]) => ({ type: k, value: v }));
        }
    } catch { /* ignore */ }

    if (entries.length === 0) {
        window.addContactRow(containerId);
    } else {
        entries.forEach(e => window.addContactRow(containerId, e.type || '', e.value || ''));
    }
};

/**
 * Read all rows from the contacts editor and return a JSON string.
 * Skips rows where both fields are empty.
 */
window.readContactsEditor = function(containerId) {
    const container = document.getElementById(containerId);
    if (!container) return '[]';
    const rows = container.querySelectorAll('.contact-row');
    const result = [];
    rows.forEach(row => {
        const type = row.querySelector('.contact-type')?.value.trim() || '';
        const value = row.querySelector('.contact-value')?.value.trim() || '';
        if (type || value) result.push({ type, value });
    });
    return JSON.stringify(result);
};

/** Escape a string for use in an HTML attribute value */
function escAttr(str) {
    return String(str || '')
        .replace(/&/g, '&amp;')
        .replace(/"/g, '&quot;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;');
}

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
    const isManager = () => ['senior_manager', 'director', 'admin'].includes(currentUserRole);
    const isJuniorOrAbove = () => ['junior_manager', 'senior_manager', 'director', 'admin'].includes(currentUserRole);
    const isDirector = () => ['director', 'admin'].includes(currentUserRole);


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

            // Show/hide tabs
            if (isDirector()) {
                document.getElementById('tabEmployees').classList.remove('hidden');
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
        'director': 'Director',
        'admin': 'Director'
    };
    const ROLE_COLORS = {
        'junior_manager': 'rgba(99,102,241,0.15)',
        'senior_manager': 'rgba(245,158,11,0.15)',
        'director': 'rgba(139,92,246,0.2)',
        'admin': 'rgba(139,92,246,0.2)'
    };
    const ROLE_TEXT_COLORS = {
        'junior_manager': '#818cf8',
        'senior_manager': '#fbbf24',
        'director': '#c4b5fd',
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
            <button class="action-btn action-btn--edit" onclick="${editFn}">✏️ Edit</button>
            <button class="action-btn action-btn--delete" onclick="${deleteFn}">🗑️ Delete</button>
        </div>`;
    }

    // ════════════════════════════════════════════════════════════════════════════
    //  LICENSES
    // ════════════════════════════════════════════════════════════════════════════
    async function loadLicenses() {
        const tbody = document.getElementById('licensesTableBody');
        tbody.innerHTML = '<tr><td colspan="9" style="text-align:center;padding:2rem;"><div class="loader inline-loader" style="margin:0 auto;border-top-color:var(--primary);"></div></td></tr>';

        try {
            const res = await fetch('/api/licenses');
            if (!res.ok) { if (res.status === 401) { window.location.href = '/login.html'; return; } throw new Error('Failed to fetch licenses'); }
            const data = await res.json();
            licensesLoaded = true;
            tbody.innerHTML = '';

            if (data.length === 0) {
                tbody.innerHTML = '<tr><td colspan="9" style="text-align:center;padding:2rem;color:var(--text-muted);">No licenses found.</td></tr>';
                return;
            }

            data.forEach(lic => {
                const tr = document.createElement('tr');
                const modulesBadges = lic.modules
                    ? lic.modules.split(',').map(m => `<span class="status-badge">${m.trim()}</span>`).join('')
                    : '<span style="color:var(--text-muted)">None</span>';

                const actionsHtml = isManager()
                    ? `<td><div style="display:flex;gap:0.5rem;">
                        <button class="action-btn action-btn--delete" onclick="window.__deleteLicense(${lic.id})">🗑️ Delete</button>
                    </div></td>`
                    : '<td class="db-col--manager-only hidden"></td>';

                tr.innerHTML = `
                    <td style="font-weight:500;color:#c4b5fd;">#${lic.id}</td>
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

            window.__deleteLicense = (id) => {
                showConfirmDelete(`Delete license #${id}? This action cannot be undone.`, async () => {
                    try {
                        const r = await fetch(`/api/licenses/${id}`, { method: 'DELETE' });
                        if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Failed to delete'); }
                        licensesLoaded = false;
                        loadLicenses();
                    } catch(e) { showError(e.message); }
                });
            };

        } catch(err) {
            tbody.innerHTML = '<tr><td colspan="9" style="text-align:center;padding:2rem;color:var(--error);">Error loading licenses.</td></tr>';
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
            if (!res.ok) throw new Error('Failed to fetch companies');
            const data = await res.json();
            companiesLoaded = true;
            tbody.innerHTML = '';

            if (data.length === 0) {
                tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;padding:2rem;color:var(--text-muted);">No companies found.</td></tr>';
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
                        <button class="action-btn action-btn--edit" data-action="edit-company" data-key="${esc(c.companyName)}">✏️ Edit</button>
                        <button class="action-btn action-btn--delete" data-action="delete-company" data-key="${esc(c.companyName)}">🗑️ Delete</button>
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
                    showConfirmDelete(`Delete company "${key}"? Associated licenses may also be affected.`, async () => {
                        try {
                            const r = await fetch(`/api/companies/${encodeURIComponent(key)}`, { method: 'DELETE' });
                            if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Failed to delete'); }
                            companiesLoaded = false;
                            loadCompanies();
                        } catch(e2) { showError(e2.message); }
                    });
                }
            }, /* persistent listener */); // no { once: true } — must work for all rows

        } catch(err) {
            tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;padding:2rem;color:var(--error);">Error loading companies.</td></tr>';
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
        btn.disabled = true; btn.textContent = 'Saving...';

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
            if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Failed to update'); }
            editCompanyModal.classList.add('hidden');
            companiesLoaded = false;
            loadCompanies();
        } catch(err) {
            showFormError('editCompanyErrorMsg', 'editCompanyError', err.message);
        } finally {
            btn.disabled = false; btn.textContent = 'Save Changes';
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
        btn.disabled = true; btn.textContent = 'Adding...';

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
            if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Failed to add company'); }
            addCompanyDbModal.classList.add('hidden');
            companiesLoaded = false;
            loadCompanies();
        } catch(err) {
            showFormError('addDbCompanyErrorMsg', 'addDbCompanyError', err.message);
        } finally {
            btn.disabled = false; btn.textContent = 'Add Company';
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
                if (res.status === 403) { tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;padding:2rem;color:var(--error);">Access denied.</td></tr>'; return; }
                throw new Error('Failed to fetch employees');
            }
            const data = await res.json();
            employeesLoaded = true;
            tbody.innerHTML = '';

            if (data.length === 0) {
                tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;padding:2rem;color:var(--text-muted);">No employees found.</td></tr>';
                return;
            }

            data.forEach(emp => {
                const tr = document.createElement('tr');
                employeeStore.set(emp.employeeId, emp); // store for later lookup

                tr.innerHTML = `
                    <td style="font-family:monospace;color:#c4b5fd;">${esc(emp.employeeId)}</td>
                    <td style="font-weight:500;">${esc(emp.login)}</td>
                    <td>${roleBadge(emp.role)}</td>
                    <td>${esc(emp.lastName)}</td>
                    <td>${esc(emp.firstName)}</td>
                    <td style="color:var(--text-muted);">${esc(emp.middleName || '—')}</td>
                    <td><div style="display:flex;gap:0.5rem;">
                        <button class="action-btn action-btn--edit" data-action="edit-employee" data-key="${esc(emp.employeeId)}">✏️ Edit</button>
                        <button class="action-btn action-btn--delete" data-action="delete-employee" data-key="${esc(emp.employeeId)}" data-login="${esc(emp.login)}">🗑️ Delete</button>
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
                    document.getElementById('editEmpRole').value = (emp.role === 'admin') ? 'director' : emp.role;
                    document.getElementById('editEmpFirstName').value = emp.firstName;
                    document.getElementById('editEmpLastName').value = emp.lastName;
                    document.getElementById('editEmpMiddleName').value = emp.middleName || '';
                    document.getElementById('editEmployeeTitle').textContent = `Edit Employee: ${emp.login}`;
                    hideFormError('editEmployeeError');
                    document.getElementById('editEmployeeModal').classList.remove('hidden');
                }

                if (action === 'delete-employee') {
                    const login = btn.dataset.login;
                    showConfirmDelete(`Delete employee "${login}" (ID: ${key})? Their account will also be removed.`, async () => {
                        try {
                            const r = await fetch(`/api/employees/${encodeURIComponent(key)}`, { method: 'DELETE' });
                            if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Failed to delete'); }
                            employeesLoaded = false;
                            loadEmployees();
                        } catch(e2) { showError(e2.message); }
                    });
                }
            }, /* persistent listener */); // no { once: true } — must work for all rows

        } catch(err) {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;padding:2rem;color:var(--error);">Error loading employees.</td></tr>';
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
        document.getElementById('editEmployeeTitle').textContent = 'Add New Employee';
        document.getElementById('editEmployeeSubmitBtn').textContent = 'Create Employee';
        hideFormError('editEmployeeError');
        editEmployeeModal.classList.remove('hidden');
    });

    document.getElementById('editEmployeeForm').addEventListener('submit', async (e) => {
        e.preventDefault();
        hideFormError('editEmployeeError');
        const btn = document.getElementById('editEmployeeSubmitBtn');
        btn.disabled = true; btn.textContent = 'Saving...';

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
            if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Failed to save employee'); }
            editEmployeeModal.classList.add('hidden');
            employeesLoaded = false;
            loadEmployees();
        } catch(err) {
            showFormError('editEmployeeErrorMsg', 'editEmployeeError', err.message);
        } finally {
            btn.disabled = false;
            btn.textContent = empId ? 'Save Employee' : 'Create Employee';
        }
    });

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
