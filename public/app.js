document.addEventListener('DOMContentLoaded', () => {
    const generateForm = document.getElementById('generateForm');
    const generateBtn = document.getElementById('generateBtn');
    const btnText = document.querySelector('.btn-text');
    const btnLoader = document.getElementById('btnLoader');
    const resultArea = document.getElementById('resultArea');
    const signatureOutput = document.getElementById('signatureOutput');
    const errorToast = document.getElementById('errorToast');
    const errorMessage = document.getElementById('errorMessage');
    const copyBtn = document.getElementById('copyBtn');
    const logoutBtn = document.getElementById('logoutBtn');

    let currentUserRole = '';

    // ─── Единый показ ошибок ─────────────────────────────────────────────────
    function showError(msg) {
        errorMessage.textContent = msg;
        errorToast.classList.remove('hidden');
        setTimeout(() => errorToast.classList.add('hidden'), 6000);
    }

    function showFormError(msgSpan, boxEl, msg) {
        msgSpan.textContent = msg;
        boxEl.classList.remove('hidden');
    }

    // Check if logged in & get role
    fetch('/api/me').then(async res => {
        if (!res.ok) {
            window.location.href = '/login.html';
        } else {
            const data = await res.json();
            currentUserRole = data.role;
            const isJuniorOrAbove = ['junior_manager', 'senior_manager', 'director', 'admin'].includes(currentUserRole);
            if (!isJuniorOrAbove && document.getElementById('openAddCompanyBtn')) {
                document.getElementById('openAddCompanyBtn').classList.add('hidden');
            }

        }
    });

    fetch('/api/version').then(async res => {
        if (res.ok) {
            const v = await res.json();
            const el = document.getElementById('appVersionInfo');
            if (el) el.textContent = `Версия сервера: ${v.serverVersion} | Версия библиотеки: ${v.libVersion}`;
        }
    });

    if (logoutBtn) {
        logoutBtn.addEventListener('click', () => {
            fetch('/api/logout', { method: 'POST' }).then(() => {
                window.location.href = '/login.html';
            });
        });
    }

    // Simple date formatting helper for today
    const now = new Date();
    const today = `${String(now.getDate()).padStart(2, '0')}.${String(now.getMonth() + 1).padStart(2, '0')}.${now.getFullYear()}`;
    const nextYear = new Date(now);
    nextYear.setFullYear(now.getFullYear() + 1);
    const inOneYear = `${String(nextYear.getDate()).padStart(2, '0')}.${String(nextYear.getMonth() + 1).padStart(2, '0')}.${nextYear.getFullYear()}`;

    // Prefill dates if empty
    if (!document.getElementById('issueDate').value) {
        document.getElementById('issueDate').value = today;
    }
    if (!document.getElementById('expiredDate').value) {
        document.getElementById('expiredDate').value = inOneYear;
    }

    const companySelect = document.getElementById('companyName');

    // Load companies
    const loadCompanies = async () => {
        try {
            const res = await fetch('/api/companies');
            if (res.ok) {
                const companies = await res.json();
                companySelect.innerHTML = '<option value="" disabled selected>Выберите компанию...</option>';
                companies.forEach(c => {
                    const opt = document.createElement('option');
                    opt.value = c.companyName;
                    opt.textContent = c.companyName;
                    companySelect.appendChild(opt);
                });
            }
        } catch (e) {
            console.error('Failed to load companies', e);
        }
    };
    loadCompanies();

    // ─── Modules Tree ─────────────────────────────────────────────────────────

    const modulesContainer = document.getElementById('modulesContainer');

    /**
     * Жёсткая структура двух групп модулей.
     *
     * Группа "SDR_receiver":
     *   [x] SDR_receiver   ← заголовок (родительский модуль)
     *       [x] SDR_analisis
     *       [x] Monopulse   ← связан с Monopulse в группе Tracking
     *
     * Группа "Tracking":
     *   [x] Tracking        ← заголовок (родительский модуль)
     *       [x] Monopulse   ← связан с Monopulse в группе SDR_receiver
     *       [x] TLE
     *
     * Правила привязки:
     *  1. При выборе любого дочернего модуля автоматически выбирается заголовок его группы.
     *  2. При снятии заголовка снимаются все дочерние модули его группы.
     *  3. Моноимпульсный (Monopulse) синхронизируется в обоих группах:
     *     выбор/снятие в одной группе — автоматически повторяется в другой.
     */
    function renderModuleTree(availableModuleNames) {
        modulesContainer.innerHTML = '';

        // Жёсткое описание дерева
        const TREE = [
            {
                parentKey: 'SDR_receiver',
                parentLabel: 'SDR_приёмник',
                children: [
                    { key: 'SDR_analisis', label: 'Анализатор спектра' },
                    { key: 'Monopulse',    label: 'Моноимпульсный' }
                ]
            },
            {
                parentKey: 'Tracking',
                parentLabel: 'Трэкинг',
                children: [
                    { key: 'Monopulse', label: 'Моноимпульсный' },
                    { key: 'TLE',       label: 'TLE' }
                ]
            }
        ];

        // Фильтруем только те модули, которые реально есть в БД
        const available = new Set(availableModuleNames);

        // Словарь: key → массив всех checkbox-ов с таким value (для синхронизации Monopulse)
        const cbByKey = {};

        const registerCb = (key, cb) => {
            if (!cbByKey[key]) cbByKey[key] = [];
            cbByKey[key].push(cb);
        };

        // Сначала создаём все элементы, потом навешиваем слушатели
        const groupMeta = []; // { parentCb, childCbs }

        for (const group of TREE) {
            // Пропускаем группу если родитель отсутствует в БД
            if (!available.has(group.parentKey)) continue;

            const groupEl = document.createElement('div');
            groupEl.className = 'module-group';

            // ── Заголовочная строка ──────────────────────────────────────
            const headerRow = document.createElement('div');
            headerRow.className = 'module-group-header-row';

            const headerLabel = document.createElement('label');
            headerLabel.className = 'module-group-header';

            const headerCb = document.createElement('input');
            headerCb.type = 'checkbox';
            headerCb.className = 'module-header-cb';
            headerCb.name = 'modules';
            headerCb.value = group.parentKey;
            headerCb.dataset.groupParent = group.parentKey;

            const headerMark = document.createElement('span');
            headerMark.className = 'checkmark';

            const headerText = document.createElement('span');
            headerText.textContent = group.parentLabel;

            headerLabel.appendChild(headerCb);
            headerLabel.appendChild(headerMark);
            headerLabel.appendChild(headerText);
            headerRow.appendChild(headerLabel);
            groupEl.appendChild(headerRow);

            registerCb(group.parentKey, headerCb);

            // ── Дочерние модули ──────────────────────────────────────────
            const childrenEl = document.createElement('div');
            childrenEl.className = 'module-group-children';
            const childCbs = [];

            for (const child of group.children) {
                if (!available.has(child.key)) continue;

                const childRow = document.createElement('div');
                childRow.className = 'module-child';

                const childLabel = document.createElement('label');
                childLabel.className = 'module-checkbox module-child-label';

                const childCb = document.createElement('input');
                childCb.type = 'checkbox';
                childCb.name = 'modules';
                childCb.value = child.key;
                childCb.dataset.groupChild = group.parentKey;

                const childMark = document.createElement('span');
                childMark.className = 'checkmark';

                const childText = document.createElement('span');
                childText.textContent = child.label;

                childLabel.appendChild(childCb);
                childLabel.appendChild(childMark);
                childLabel.appendChild(childText);
                childRow.appendChild(childLabel);
                childrenEl.appendChild(childRow);
                childCbs.push(childCb);

                registerCb(child.key, childCb);
            }

            groupEl.appendChild(childrenEl);
            modulesContainer.appendChild(groupEl);

            groupMeta.push({ parentKey: group.parentKey, parentCb: headerCb, childCbs });
        }

        if (!groupMeta.length) {
            modulesContainer.innerHTML =
                '<p style="color:var(--text-muted);font-size:0.9rem;padding:0.5rem">Нет доступных модулей.</p>';
            return;
        }

        // ── Навешиваем слушатели после построения DOM ────────────────────

        // Вспомогательная: синхронизировать все cb с одинаковым key (cross-group)
        const syncKey = (key, checked, originCb) => {
            (cbByKey[key] || []).forEach(cb => {
                if (cb !== originCb) cb.checked = checked;
            });
        };

        // Правило 1: дочерний выбран → заголовок выбирается автоматически
        // Правило 2: заголовок снят  → все дочерние снимаются
        // Правило 3: Monopulse синхронизируется между группами
        for (const meta of groupMeta) {
            // Слушатель на каждый дочерний чекбокс
            for (const childCb of meta.childCbs) {
                childCb.addEventListener('change', () => {
                    if (childCb.checked) {
                        // Правило 1: включить заголовок
                        meta.parentCb.checked = true;
                        syncKey(meta.parentKey, true, meta.parentCb);
                    }
                    // Правило 3: синхронизация cross-group
                    syncKey(childCb.value, childCb.checked, childCb);

                    // Если это Monopulse и он включён — включаем заголовок в другой группе тоже
                    if (childCb.value === 'Monopulse' && childCb.checked) {
                        (cbByKey['Monopulse'] || []).forEach(cb => {
                            if (cb !== childCb && cb.dataset.groupChild) {
                                const otherParentKey = cb.dataset.groupChild;
                                (cbByKey[otherParentKey] || []).forEach(pcb => {
                                    pcb.checked = true;
                                });
                            }
                        });
                    }
                });
            }

            // Слушатель на заголовочный чекбокс
            meta.parentCb.addEventListener('change', () => {
                if (!meta.parentCb.checked) {
                    // Правило 2: снять все дочерние
                    meta.childCbs.forEach(cb => {
                        cb.checked = false;
                        syncKey(cb.value, false, cb);
                    });
                }
                syncKey(meta.parentKey, meta.parentCb.checked, meta.parentCb);
            });
        }
    }

    async function loadModules() {
        try {
            const res = await fetch('/api/modules');
            if (!res.ok) throw new Error('Не удалось загрузить список модулей');
            const names = await res.json();
            renderModuleTree(names);
        } catch (e) {
            modulesContainer.innerHTML =
                `<p style="color:var(--error);font-size:0.9rem;padding:0.5rem">${e.message}</p>`;
            console.error('loadModules error:', e);
        }
    }
    loadModules();

    // Modal logic
    const addCompanyModal = document.getElementById('addCompanyModal');
    const openAddCompanyBtn = document.getElementById('openAddCompanyBtn');
    const closeCompanyModal = document.getElementById('closeCompanyModal');
    const addCompanyForm = document.getElementById('addCompanyForm');

    openAddCompanyBtn.addEventListener('click', () => {
        document.getElementById('newCompanyName').value = '';
        document.getElementById('newCompanyCity').value = '';
        // Initialize contacts editor with one empty row
        if (window.populateContactsEditor) {
            window.populateContactsEditor('addCompanyContactsEditor', null);
        }
        const errBox = document.getElementById('companyError');
        if (errBox) errBox.classList.add('hidden');
        addCompanyModal.classList.remove('hidden');
    });

    closeCompanyModal.addEventListener('click', () => {
        addCompanyModal.classList.add('hidden');
    });

    // Close if clicked outside
    window.addEventListener('click', (e) => {
        if (e.target === addCompanyModal) {
            addCompanyModal.classList.add('hidden');
        }
    });

    addCompanyForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const formData = new FormData(addCompanyForm);
        const data = {
            companyName: formData.get('companyName'),
            city: formData.get('city'),
            contacts: window.readContactsEditor ? window.readContactsEditor('addCompanyContactsEditor') : '[]'
        };

        try {
            const btn = document.getElementById('addCompanySubmitBtn');
            btn.disabled = true;
            btn.textContent = 'Добавление...';

            const response = await fetch('/api/companies', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });

            const result = await response.json();
            if (!response.ok) {
                throw new Error(result.error || 'Не удалось добавить компанию.');
            }

            // Success: reload companies & select the new one
            await loadCompanies();
            companySelect.value = data.companyName;

            // Reset fields manually (cannot use form.reset() as contacts editor is not a form input)
            document.getElementById('newCompanyName').value = '';
            document.getElementById('newCompanyCity').value = '';
            if (window.populateContactsEditor) window.populateContactsEditor('addCompanyContactsEditor', null);
            addCompanyModal.classList.add('hidden');

            // Show a success message (reusing the toast for now but styled green maybe)
            // Or just do nothing.

        } catch (error) {
            const errBox = document.getElementById('companyError');
            const errMsg = document.getElementById('companyErrorMessage');
            if (errBox && errMsg) {
                showFormError(errMsg, errBox, error.message);
            } else {
                showError(error.message);
            }
        } finally {
            const btn = document.getElementById('addCompanySubmitBtn');
            btn.disabled = false;
            btn.textContent = 'Добавить компанию';
        }
    });

    let currentLicenseData = null;


    generateForm.setAttribute('novalidate', '');

    generateForm.addEventListener('submit', async (e) => {
        e.preventDefault();

        // ── Ручная клиентская валидация (вместо браузерных всплывашек) ───────
        const companyVal = generateForm.elements['companyName'].value;
        const hwVal = generateForm.elements['hardwareId'].value.trim();
        const issueDateVal = generateForm.elements['issueDate'].value.trim();
        const expiredDateVal = generateForm.elements['expiredDate'].value.trim();
        const modulesChecked = generateForm.querySelectorAll('input[name="modules"]:checked').length > 0;

        if (!companyVal) {
            showError('Пожалуйста, выберите компанию из списка.');
            return;
        }
        if (!hwVal) {
            showError('Пожалуйста, заполните поле Hardware ID.');
            return;
        }
        if (!issueDateVal) {
            showError('Пожалуйста, укажите дату выдачи.');
            return;
        }
        if (!expiredDateVal) {
            showError('Пожалуйста, укажите дату окончания.');
            return;
        }
        if (!modulesChecked) {
            showError('Пожалуйста, выберите предоставляемые модули.');
            return;
        }

        // Reset UI
        resultArea.classList.add('hidden');
        errorToast.classList.add('hidden');
        btnText.classList.add('hidden');
        btnLoader.classList.remove('hidden');
        generateBtn.disabled = true;

        // Gather data
        const formData = new FormData(generateForm);
        const data = {
            companyName: formData.get('companyName'),
            hardwareId: formData.get('hardwareId'),
            issueDate: formData.get('issueDate'),
            expiredDate: formData.get('expiredDate'),
            // Deduplicate: Monopulse appears in both groups but must be sent once
            modules: [...new Set(formData.getAll('modules'))]
        };

        try {
            // Client-side date validation
            const parseDate = (dateStr) => {
                const parts = dateStr.split('.');
                if (parts.length !== 3) return null;
                // new Date(year, monthIndex, day)
                return new Date(parseInt(parts[2]), parseInt(parts[1]) - 1, parseInt(parts[0]));
            };

            const issueDateObj = parseDate(data.issueDate);
            const expiredDateObj = parseDate(data.expiredDate);

            if (!issueDateObj || !expiredDateObj) {
                throw new Error("Неверный формат даты. Используйте дд.мм.гггг");
            }

            if (issueDateObj >= expiredDateObj) {
                throw new Error("Дата выдачи должна быть раньше даты окончания.");
            }
            const response = await fetch('/api/generate', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(data)
            });

            const result = await response.json();

            if (!response.ok) {
                if (response.status === 401) {
                    window.location.href = '/login.html';
                    return;
                }
                throw new Error(result.error || 'Во время генерации произошла ошибка сервера.');
            }

            // Success
            signatureOutput.textContent = result.signature;
            currentLicenseData = {
                licenseDesc: {
                    company: data.companyName,
                    expiredDate: data.expiredDate,
                    issueDate: data.issueDate,
                    modules: data.modules,
                    signature: result.signature
                }
            };
            resultArea.classList.remove('hidden');

        } catch (error) {
            showError(error.message);
        } finally {
            // Restore button
            btnText.classList.remove('hidden');
            btnLoader.classList.add('hidden');
            generateBtn.disabled = false;
        }
    });

    copyBtn.addEventListener('click', () => {
        const text = signatureOutput.textContent;
        navigator.clipboard.writeText(text).then(() => {
            const originalTitle = copyBtn.getAttribute('title');
            copyBtn.setAttribute('title', 'Скопировано!');
            copyBtn.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#10b981" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg>';

            setTimeout(() => {
                copyBtn.setAttribute('title', originalTitle);
                copyBtn.innerHTML = '<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path></svg>';
            }, 2000);
        });
    });

    const downloadJsonBtn = document.getElementById('downloadJsonBtn');
    if (downloadJsonBtn) {
        downloadJsonBtn.addEventListener('click', () => {
            if (!currentLicenseData) return;
            const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(currentLicenseData, null, 4));
            const downloadAnchorNode = document.createElement('a');
            downloadAnchorNode.setAttribute("href", dataStr);
            downloadAnchorNode.setAttribute("download", "license_" + currentLicenseData.licenseDesc.company.replace(/[^a-z0-9]/gi, '_').toLowerCase() + ".json");
            document.body.appendChild(downloadAnchorNode);
            downloadAnchorNode.click();
            downloadAnchorNode.remove();
        });
    }
});
