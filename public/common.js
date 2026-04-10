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
        <input type="text" class="contact-type" placeholder="Тип контакта (например, Телефон)" value="${escAttr(type)}" autocomplete="off">
        <input type="text" class="contact-value" placeholder="Значение (например, +7 999 123-45-67)" value="${escAttr(value)}" autocomplete="off">
        <button type="button" class="contact-remove-btn" title="Удалить" onclick="this.closest('.contact-row').remove()">✕</button>
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
window.escAttr = function(str) {
    return String(str || '')
        .replace(/&/g, '&amp;')
        .replace(/"/g, '&quot;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;');
};
