document.addEventListener('DOMContentLoaded', function() {
    const wifiSelect = document.getElementById('wifiNetwork');
    const passwordGroup = document.getElementById('passwordGroup');
    const passwordInput = document.getElementById('wifiPassword');
    const showPasswordCheckbox = document.getElementById('showPassword');
    const wifiForm = document.getElementById('wifiForm');
    const refreshBtn = document.getElementById('refreshBtn');
    const statusMessage = document.getElementById('statusMessage');
    
    // Список доступных Wi-Fi сетей (в реальности получается через API)
    let wifiNetworks = [];
    
    // Имитация получения Wi-Fi сетей
    function loadWifiNetworks() {
        statusMessage.className = 'status loading';
        statusMessage.textContent = 'Сканирование Wi-Fi сетей...';
        statusMessage.classList.remove('hidden');
        
        // Имитация задержки сети
        setTimeout(() => {
            wifiNetworks = [
            ];
            
            updateWifiDropdown();
            
            statusMessage.classList.add('hidden');
        }, 1500);
    }
    
    function updateWifiDropdown() {
        wifiSelect.innerHTML = '<option value="">Выберите сеть...</option>';
        
        wifiNetworks.forEach((network, index) => {
            const option = document.createElement('option');
            option.value = network.ssid;
            
            // Отображаем уровень сигнала
            let signalBars = '📶';
            if (network.signal > 70) signalBars = '📶📶📶';
            else if (network.signal > 40) signalBars = '📶📶';
            
            // Отображаем тип защиты
            let securityIcon = network.security === 'Open' ? '🔓' : '🔒';
            
            option.textContent = `${securityIcon} ${network.ssid} ${signalBars}`;
            option.dataset.security = network.security;
            wifiSelect.appendChild(option);
        });
    }
    
    // Показать/скрыть поле для пароля
    wifiSelect.addEventListener('change', function() {
        const selectedOption = this.options[this.selectedIndex];
        const security = selectedOption.dataset.security;
        
        if (security && security !== 'Open') {
            passwordGroup.classList.remove('hidden');
            passwordInput.required = true;
        } else {
            passwordGroup.classList.add('hidden');
            passwordInput.required = false;
            passwordInput.value = '';
        }
    });
    
    // Показать/скрыть пароль
    showPasswordCheckbox.addEventListener('change', function() {
        passwordInput.type = this.checked ? 'text' : 'password';
    });
    
    // Обновить список сетей
    refreshBtn.addEventListener('click', loadWifiNetworks);
    
    // Обработка отправки формы
    wifiForm.addEventListener('submit', function(e) {
        e.preventDefault();
        
        const selectedNetwork = wifiSelect.value;
        const password = passwordInput.value;
        const security = wifiSelect.options[wifiSelect.selectedIndex].dataset.security;
        
        if (!selectedNetwork) {
            showStatus('Пожалуйста, выберите Wi-Fi сеть', 'error');
            return;
        }
        
        if (security !== 'Open' && !password) {
            showStatus('Для этой сети требуется пароль', 'error');
            return;
        }
        
        showStatus(`Подключение к сети "${selectedNetwork}"...`, 'loading');
        
        // Имитация подключения
        setTimeout(() => {
            // В реальном приложении здесь будет вызов API для подключения
            showStatus(`Успешно подключено к "${selectedNetwork}"!`, 'success');
            
            // Сброс формы
            setTimeout(() => {
                wifiForm.reset();
                passwordGroup.classList.add('hidden');
                statusMessage.classList.add('hidden');
            }, 3000);
        }, 2000);
    });
    
    function showStatus(message, type) {
        statusMessage.textContent = message;
        statusMessage.className = `status ${type}`;
        statusMessage.classList.remove('hidden');
    }
    
    // Загрузить сети при загрузке страницы
    loadWifiNetworks();
});