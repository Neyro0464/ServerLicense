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
    let allConfigsData = [];  // cached configurations
    let allModulesData = [];  // cached modules from API

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
            const isJuniorOrAbove = ['junior_manager', 'senior_manager', 'admin'].includes(currentUserRole);
            if (!isJuniorOrAbove && document.getElementById('openAddCompanyBtn')) {
                document.getElementById('openAddCompanyBtn').classList.add('hidden');
            }
            // Show config add button only for admin
            if (currentUserRole === 'admin' && document.getElementById('openAddConfigBtn')) {
                document.getElementById('openAddConfigBtn').classList.remove('hidden');
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

    // ─── Modules Tree (dynamic from DB) ────────────────────────────────────────

    const modulesContainer = document.getElementById('modulesContainer');
    const modulesBlock = document.getElementById('modulesBlock');
    const systemConfigSelect = document.getElementById('systemConfig');

    /**
     * Builds module tree dynamically from API data.
     * Parent modules (parentModule === '') become group headers.
     * Child modules are nested under their parent.
     * Only modules in allowedNames are shown (including all descendants).
     * Handles: isSelectable, requiresDevice, dependencyGroup, requiredWith
     */
    function renderModuleTree(moduleRecords, allowedNames) {
        modulesContainer.innerHTML = '';
        const allowed = new Set(allowedNames);

        // Add parent modules to maintain hierarchy structure
        // If a child module is in the config, we need to show its parent for proper display
        const addParents = (moduleName) => {
            const mod = moduleRecords.find(m => m.moduleName === moduleName);
            if (mod && mod.parentModule && !allowed.has(mod.parentModule)) {
                allowed.add(mod.parentModule);
                addParents(mod.parentModule); // Recursively add parent chain
            }
        };

        // Add all parent modules for proper hierarchy display
        allowedNames.forEach(name => addParents(name));

        const filtered = moduleRecords.filter(m => allowed.has(m.moduleName));

        // Build hierarchy map
        const childrenMap = {};
        filtered.forEach(m => {
            if (m.parentModule) {
                if (!childrenMap[m.parentModule]) childrenMap[m.parentModule] = [];
                childrenMap[m.parentModule].push(m);
            }
        });

        // Find root modules (no parent or parent not in filtered set)
        const parents = filtered.filter(m => !m.parentModule || !allowed.has(m.parentModule));

        const cbByKey = {};
        const moduleByKey = {};
        filtered.forEach(m => moduleByKey[m.moduleName] = m);

        const registerCb = (key, cb) => {
            if (!cbByKey[key]) cbByKey[key] = [];
            cbByKey[key].push(cb);
        };
        const groupMeta = [];

        // Recursive function to render module and its children
        function renderModuleWithChildren(mod, level = 0) {
            const groupEl = document.createElement('div');
            groupEl.className = 'module-group';
            groupEl.style.marginLeft = (level * 20) + 'px';

            // Header
            const headerRow = document.createElement('div');
            headerRow.className = 'module-group-header-row';
            const headerLabel = document.createElement('label');
            headerLabel.className = 'module-group-header';

            let headerCb = null;
            const childCbs = [];

            // Parent checkbox (or just label if not selectable)
            if (mod.isSelectable) {
                headerCb = document.createElement('input');
                headerCb.type = 'checkbox';
                headerCb.className = 'module-header-cb';
                headerCb.name = 'modules';
                headerCb.value = mod.moduleName;
                headerCb.dataset.groupParent = mod.moduleName;
                const headerMark = document.createElement('span');
                headerMark.className = 'checkmark';
                const headerText = document.createElement('span');
                headerText.textContent = mod.displayLabel || mod.moduleName;
                if (mod.requiresDevice) {
                    headerText.textContent += ' 🖥️';
                    headerText.title = 'Требует устройство';
                }
                headerLabel.append(headerCb, headerMark, headerText);
                registerCb(mod.moduleName, headerCb);
            } else {
                // Non-selectable parent (category only)
                const headerText = document.createElement('span');
                headerText.textContent = mod.displayLabel || mod.moduleName;
                headerText.style.fontWeight = 'bold';
                headerText.style.color = 'var(--text-muted)';
                headerLabel.appendChild(headerText);
            }

            headerRow.appendChild(headerLabel);
            groupEl.appendChild(headerRow);

            // Children container
            const childrenEl = document.createElement('div');
            childrenEl.className = 'module-group-children';
            const children = (childrenMap[mod.moduleName] || []).sort((a, b) => a.sortOrder - b.sortOrder);

            // Group children by dependency group for radio buttons
            const depGroups = {};
            children.forEach(child => {
                if (child.dependencyGroup) {
                    if (!depGroups[child.dependencyGroup]) depGroups[child.dependencyGroup] = [];
                    depGroups[child.dependencyGroup].push(child);
                }
            });

            for (const child of children) {
                if (!child.isSelectable) {
                    // Non-selectable child - render as nested group
                    const nestedGroup = renderModuleWithChildren(child, level + 1);
                    childrenEl.appendChild(nestedGroup);
                    continue;
                }

                const childRow = document.createElement('div');
                childRow.className = 'module-child';
                const childLabel = document.createElement('label');
                childLabel.className = 'module-checkbox module-child-label';

                const childCb = document.createElement('input');
                // Use radio button if part of dependency group
                if (child.dependencyGroup && depGroups[child.dependencyGroup].length > 1) {
                    childCb.type = 'radio';
                    childCb.name = `depgroup_${child.dependencyGroup}`;
                } else {
                    childCb.type = 'checkbox';
                    childCb.name = 'modules';
                }
                childCb.value = child.moduleName;
                childCb.dataset.groupChild = mod.moduleName;
                if (child.dependencyGroup) childCb.dataset.dependencyGroup = child.dependencyGroup;
                if (child.requiredWith && child.requiredWith.length > 0) {
                    childCb.dataset.requiredWith = JSON.stringify(child.requiredWith);
                }

                const childMark = document.createElement('span');
                childMark.className = 'checkmark';
                const childText = document.createElement('span');
                childText.textContent = child.displayLabel || child.moduleName;
                if (child.requiresDevice) {
                    childText.textContent += ' 🖥️';
                    childText.title = 'Требует устройство';
                }
                if (child.description) {
                    childText.title = child.description;
                }

                childLabel.append(childCb, childMark, childText);
                childRow.appendChild(childLabel);
                childrenEl.appendChild(childRow);
                childCbs.push(childCb);
                registerCb(child.moduleName, childCb);
            }

            groupEl.appendChild(childrenEl);
            groupMeta.push({ parentKey: mod.moduleName, parentCb: headerCb, childCbs: childCbs, parentModule: mod });
            return groupEl;
        }

        // Render all root modules
        for (const parent of parents) {
            const groupEl = renderModuleWithChildren(parent, 0);
            modulesContainer.appendChild(groupEl);
        }

        if (!groupMeta.length) {
            modulesContainer.innerHTML = '<p style="color:var(--text-muted);font-size:0.9rem;padding:0.5rem">Нет доступных модулей для этой конфигурации.</p>';
            return;
        }

        // Sync logic with validation
        const syncKey = (key, checked, originCb) => {
            (cbByKey[key] || []).forEach(cb => { if (cb !== originCb) cb.checked = checked; });
        };

        const autoSelectRequired = (moduleName) => {
            const mod = moduleByKey[moduleName];
            if (!mod || !mod.requiredWith || mod.requiredWith.length === 0) return;

            mod.requiredWith.forEach(reqName => {
                const reqCbs = cbByKey[reqName];
                if (reqCbs) {
                    reqCbs.forEach(cb => {
                        if (!cb.checked) {
                            cb.checked = true;
                            syncKey(reqName, true, cb);
                            // Recursively check required dependencies
                            autoSelectRequired(reqName);
                            // Check parent if child is selected
                            if (cb.dataset.groupChild) {
                                const parentKey = cb.dataset.groupChild;
                                const parentMeta = groupMeta.find(m => m.parentKey === parentKey);
                                if (parentMeta && parentMeta.parentCb) {
                                    parentMeta.parentCb.checked = true;
                                    syncKey(parentKey, true, parentMeta.parentCb);
                                }
                            }
                        }
                    });
                }
            });
        };

        for (const meta of groupMeta) {
            for (const childCb of meta.childCbs) {
                childCb.addEventListener('change', () => {
                    if (childCb.checked) {
                        // Auto-select parent
                        if (meta.parentCb) {
                            meta.parentCb.checked = true;
                            syncKey(meta.parentKey, true, meta.parentCb);
                        }
                        // Auto-select required modules
                        autoSelectRequired(childCb.value);
                    }
                    syncKey(childCb.value, childCb.checked, childCb);
                });
            }

            if (meta.parentCb) {
                meta.parentCb.addEventListener('change', () => {
                    if (!meta.parentCb.checked) {
                        // Uncheck all children
                        meta.childCbs.forEach(cb => {
                            cb.checked = false;
                            syncKey(cb.value, false, cb);
                        });
                    }
                    syncKey(meta.parentKey, meta.parentCb.checked, meta.parentCb);
                });
            }
        }
    }

    // Load modules + configurations
    async function loadModulesAndConfigs() {
        try {
            // Load modules
            const modRes = await fetch('/api/modules');
            if (!modRes.ok) throw new Error('Не удалось загрузить модули');
            allModulesData = await modRes.json();

            // Load configurations
            const cfgRes = await fetch('/api/configurations');
            if (!cfgRes.ok) throw new Error('Не удалось загрузить конфигурации');
            allConfigsData = await cfgRes.json();

            // Populate config select
            systemConfigSelect.innerHTML = '<option value="" disabled selected>Выберите конфигурацию...</option>';
            allConfigsData.forEach(cfg => {
                const opt = document.createElement('option');
                opt.value = cfg.configId;
                opt.textContent = cfg.configName;
                systemConfigSelect.appendChild(opt);
            });
        } catch (e) {
            console.error('loadModulesAndConfigs error:', e);
        }
    }
    loadModulesAndConfigs();

    // When config changes, show only allowed modules and reset checkboxes
    systemConfigSelect.addEventListener('change', () => {
        const selectedId = parseInt(systemConfigSelect.value);
        const cfg = allConfigsData.find(c => c.configId === selectedId);
        if (!cfg) return;

        modulesBlock.style.display = '';
        renderModuleTree(allModulesData, cfg.modules || []);
    });


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
        const configVal = systemConfigSelect ? systemConfigSelect.value : '1';

        if (!companyVal) {
            showError('Пожалуйста, выберите компанию из списка.');
            return;
        }
        if (!configVal) {
            showError('Пожалуйста, выберите конфигурацию системы.');
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

        // Validate that only selectable modules are checked
        const checkedModules = Array.from(generateForm.querySelectorAll('input[name="modules"]:checked, input[type="radio"]:checked'))
            .map(cb => cb.value);

        const nonSelectableChecked = checkedModules.filter(modName => {
            const mod = allModulesData.find(m => m.moduleName === modName);
            return mod && !mod.isSelectable;
        });

        if (nonSelectableChecked.length > 0) {
            showError(`Следующие модули не могут быть выбраны: ${nonSelectableChecked.join(', ')}`);
            return;
        }

        // Check for category-only selections (parent without children)
        const selectedSet = new Set(checkedModules);
        const warnings = [];

        allModulesData.forEach(mod => {
            if (selectedSet.has(mod.moduleName) && !mod.parentModule) {
                // This is a parent module, check if any children are selected
                const children = allModulesData.filter(m => m.parentModule === mod.moduleName && m.isSelectable);
                const hasSelectedChild = children.some(child => selectedSet.has(child.moduleName));
                if (!hasSelectedChild && children.length > 0) {
                    warnings.push(`Модуль "${mod.displayLabel || mod.moduleName}" выбран без дочерних модулей.`);
                }
            }
        });

        if (warnings.length > 0) {
            const proceed = confirm(warnings.join('\n') + '\n\nПродолжить генерацию?');
            if (!proceed) return;
        }

        // Reset UI
        resultArea.classList.add('hidden');
        errorToast.classList.add('hidden');
        btnText.classList.add('hidden');
        btnLoader.classList.remove('hidden');
        generateBtn.disabled = true;

        // Gather data
        const formData = new FormData(generateForm);

        // Collect modules from both checkboxes and radio buttons
        const selectedModules = [];

        // Get checkbox modules
        generateForm.querySelectorAll('input[name="modules"]:checked').forEach(cb => {
            selectedModules.push(cb.value);
        });

        // Get radio button modules (dependency groups)
        generateForm.querySelectorAll('input[type="radio"]:checked').forEach(rb => {
            if (rb.name.startsWith('depgroup_')) {
                selectedModules.push(rb.value);
            }
        });

        const data = {
            companyName: formData.get('companyName'),
            hardwareId: formData.get('hardwareId'),
            issueDate: formData.get('issueDate'),
            expiredDate: formData.get('expiredDate'),
            note: formData.get('note') || '',
            // Deduplicate modules
            modules: [...new Set(selectedModules)]
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
            downloadAnchorNode.setAttribute("download", "license_" + currentLicenseData.licenseDesc.company.replace(/[^a-z0-9а-яё]+/gi, '_').toLowerCase() + ".json");
            document.body.appendChild(downloadAnchorNode);
            downloadAnchorNode.click();
            downloadAnchorNode.remove();
        });
    }

    // ─── Add Configuration Modal (from generation page) ──────────────────────
    const cfgModalGen = document.getElementById('addConfigModalGen');
    const openCfgBtn = document.getElementById('openAddConfigBtn');
    if (cfgModalGen && openCfgBtn) {
        function renderGenConfigModules() {
            const cont = document.getElementById('genConfigModuleList');
            if (!cont) return;
            cont.innerHTML = '';
            for (const mod of allModulesData) {
                const lbl = document.createElement('label');
                lbl.style.cssText = 'display:flex;align-items:center;gap:0.5rem;font-size:0.9rem;cursor:pointer;color:var(--text-main)';
                const cb = document.createElement('input');
                cb.type = 'checkbox'; cb.value = mod.moduleName;
                cb.style.cssText = 'accent-color:var(--primary);width:16px;height:16px;cursor:pointer';
                const sp = document.createElement('span');
                sp.textContent = (mod.displayLabel || mod.moduleName) + (mod.parentModule ? ` (→ ${mod.parentModule})` : ' [группа]');
                lbl.append(cb, sp);
                cont.appendChild(lbl);
            }
            if (!allModulesData.length) cont.innerHTML = '<span style="color:var(--text-muted)">Модули не загружены</span>';
        }

        openCfgBtn.addEventListener('click', () => {
            document.getElementById('newConfigName').value = '';
            document.getElementById('newConfigDesc').value = '';
            document.getElementById('configErrorGen').classList.add('hidden');
            renderGenConfigModules();
            cfgModalGen.classList.remove('hidden');
        });
        document.getElementById('closeConfigModalGen').addEventListener('click', () => cfgModalGen.classList.add('hidden'));
        window.addEventListener('click', e => { if (e.target === cfgModalGen) cfgModalGen.classList.add('hidden'); });

        document.getElementById('addConfigFormGen').addEventListener('submit', async e => {
            e.preventDefault();
            const errBox = document.getElementById('configErrorGen');
            const errMsg = document.getElementById('configErrorMessageGen');
            errBox.classList.add('hidden');
            const btn = document.getElementById('addConfigSubmitBtnGen');
            btn.disabled = true; btn.textContent = 'Добавление...';

            const cont = document.getElementById('genConfigModuleList');
            const selectedMods = [...cont.querySelectorAll('input[type=checkbox]:checked')].map(c => c.value);

            try {
                const r = await fetch('/api/configurations', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        configName: document.getElementById('newConfigName').value.trim(),
                        description: document.getElementById('newConfigDesc').value.trim(),
                        modules: selectedMods
                    })
                });
                if (!r.ok) { const d = await r.json(); throw new Error(d.error || 'Ошибка'); }
                cfgModalGen.classList.add('hidden');
                // Reload configs
                await loadModulesAndConfigs();
            } catch(err) {
                errMsg.textContent = err.message;
                errBox.classList.remove('hidden');
            } finally {
                btn.disabled = false; btn.textContent = 'Добавить конфигурацию';
            }
        });
    }
});

