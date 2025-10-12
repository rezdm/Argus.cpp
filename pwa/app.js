// Argus Monitor PWA Application

class ArgusMonitor {
  constructor() {
    this.apiBaseUrl = window.location.origin;
    this.baseUrl = ''; // Will be loaded from server config
    this.statusEndpoint = '/status';
    this.subscribeEndpoint = '/push/subscribe';
    this.unsubscribeEndpoint = '/push/unsubscribe';
    this.refreshInterval = 30000; // 30 seconds
    this.refreshTimer = null;
    this.swRegistration = null;
    this.deferredPrompt = null;
    this.isOnline = navigator.onLine;

    this.init();
  }

  async init() {
    console.log('[Argus] Initializing application...');

    // Load server configuration first
    await this.loadConfig();

    // Register service worker
    await this.registerServiceWorker();

    // Setup event listeners
    this.setupEventListeners();

    // Check notification permission status
    this.updateNotificationButton();

    // Load initial data
    await this.loadStatus();

    // Start auto-refresh
    this.startAutoRefresh();

    // Check for install prompt
    this.checkInstallPrompt();

    console.log('[Argus] Application initialized');
  }

  async loadConfig() {
    try {
      const response = await fetch('config.json');
      if (response.ok) {
        const config = await response.json();
        this.baseUrl = config.base_url || '';
        console.log('[Argus] Loaded server config, base_url:', this.baseUrl);

        // Update app name if provided
        if (config.name) {
          document.getElementById('app-name').textContent = config.name;
          document.title = config.name;
        }
      }
    } catch (error) {
      console.warn('[Argus] Could not load server config, using defaults:', error.message);
      this.baseUrl = ''; // Default to no base URL
    }
  }

  async registerServiceWorker() {
    if ('serviceWorker' in navigator) {
      try {
        this.swRegistration = await navigator.serviceWorker.register('sw.js');
        console.log('[Argus] Service Worker registered:', this.swRegistration);

        // Listen for service worker updates
        this.swRegistration.addEventListener('updatefound', () => {
          console.log('[Argus] Service Worker update found');
          const newWorker = this.swRegistration.installing;
          newWorker.addEventListener('statechange', () => {
            if (newWorker.state === 'installed' && navigator.serviceWorker.controller) {
              console.log('[Argus] New Service Worker installed, reload to update');
              // Optionally show update notification
            }
          });
        });
      } catch (error) {
        console.error('[Argus] Service Worker registration failed:', error);
      }
    }
  }

  setupEventListeners() {
    // Refresh button
    document.getElementById('refresh-btn').addEventListener('click', () => {
      this.loadStatus(true);
    });

    // Retry button
    document.getElementById('retry-btn').addEventListener('click', () => {
      this.loadStatus(true);
    });

    // Notification button
    document.getElementById('notification-btn').addEventListener('click', () => {
      this.showNotificationModal();
    });

    // Install button
    document.getElementById('install-link').addEventListener('click', (e) => {
      e.preventDefault();
      this.showInstallModal();
    });

    document.getElementById('install-btn').addEventListener('click', () => {
      this.installApp();
    });

    document.getElementById('install-cancel-btn').addEventListener('click', () => {
      this.hideInstallModal();
    });

    // Notification modal
    document.getElementById('notification-enable-btn').addEventListener('click', () => {
      this.requestNotificationPermission();
    });

    document.getElementById('notification-cancel-btn').addEventListener('click', () => {
      this.hideNotificationModal();
    });

    // Modal close buttons
    document.getElementById('modal-close-btn').addEventListener('click', () => {
      this.hideInstallModal();
    });

    document.getElementById('notification-modal-close-btn').addEventListener('click', () => {
      this.hideNotificationModal();
    });

    // Online/Offline events
    window.addEventListener('online', () => {
      this.isOnline = true;
      this.updateConnectionStatus();
      this.loadStatus();
      document.getElementById('offline-banner').style.display = 'none';
    });

    window.addEventListener('offline', () => {
      this.isOnline = false;
      this.updateConnectionStatus();
      document.getElementById('offline-banner').style.display = 'block';
    });

    // Visibility change - refresh when app becomes visible
    document.addEventListener('visibilitychange', () => {
      if (!document.hidden && this.isOnline) {
        this.loadStatus();
      }
    });

    // Before install prompt
    window.addEventListener('beforeinstallprompt', (e) => {
      e.preventDefault();
      this.deferredPrompt = e;
      document.getElementById('install-prompt').style.display = 'block';
      console.log('[Argus] Install prompt available');
    });

    // App installed
    window.addEventListener('appinstalled', () => {
      console.log('[Argus] App installed');
      this.deferredPrompt = null;
      document.getElementById('install-prompt').style.display = 'none';
    });
  }

  async loadStatus(forceRefresh = false) {
    const loadingEl = document.getElementById('loading');
    const errorEl = document.getElementById('error');
    const monitorsEl = document.getElementById('monitors-container');
    const emptyEl = document.getElementById('empty-state');
    const refreshBtn = document.getElementById('refresh-btn');

    // Show loading state only on first load
    if (!forceRefresh && monitorsEl.children.length === 0) {
      loadingEl.style.display = 'block';
      errorEl.style.display = 'none';
      monitorsEl.style.display = 'none';
      emptyEl.style.display = 'none';
    }

    // Add spinning animation
    refreshBtn.classList.add('spinning');

    try {
      const response = await fetch(this.apiBaseUrl + this.baseUrl + this.statusEndpoint);

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      this.renderMonitors(data);

      // Update app name
      if (data.name) {
        document.getElementById('app-name').textContent = data.name;
        document.title = data.name;
      }

      // Update last update time
      const now = new Date();
      document.getElementById('last-update').textContent =
        `Last updated: ${now.toLocaleTimeString()}`;

      // Hide loading, show content
      loadingEl.style.display = 'none';
      errorEl.style.display = 'none';

      if (data.groups && data.groups.length > 0) {
        monitorsEl.style.display = 'block';
        emptyEl.style.display = 'none';
      } else {
        monitorsEl.style.display = 'none';
        emptyEl.style.display = 'block';
      }

    } catch (error) {
      console.error('[Argus] Failed to load status:', error);

      loadingEl.style.display = 'none';
      monitorsEl.style.display = 'none';
      emptyEl.style.display = 'none';
      errorEl.style.display = 'block';

      document.getElementById('error').querySelector('.error-message').textContent =
        `Failed to load monitor data: ${error.message}`;
    } finally {
      refreshBtn.classList.remove('spinning');
    }
  }

  renderMonitors(data) {
    const container = document.getElementById('monitors-container');
    container.innerHTML = '';

    if (!data.groups || data.groups.length === 0) {
      return;
    }

    data.groups.forEach(group => {
      const groupEl = this.createGroupElement(group);
      container.appendChild(groupEl);
    });
  }

  createGroupElement(group) {
    const groupDiv = document.createElement('div');
    groupDiv.className = 'monitor-group';

    const header = document.createElement('div');
    header.className = 'group-header';
    header.innerHTML = `
      <span class="group-icon">📁</span>
      <span>${this.escapeHtml(group.name || 'Unnamed Group')}</span>
    `;
    groupDiv.appendChild(header);

    const monitorList = document.createElement('div');
    monitorList.className = 'monitor-list';

    if (group.monitors && group.monitors.length > 0) {
      group.monitors.forEach(monitor => {
        const monitorEl = this.createMonitorElement(monitor);
        monitorList.appendChild(monitorEl);
      });
    }

    groupDiv.appendChild(monitorList);
    return groupDiv;
  }

  createMonitorElement(monitor) {
    const div = document.createElement('div');
    const status = (monitor.status || 'PENDING').toLowerCase();
    div.className = `monitor-item status-${status}`;

    const statusIcon = this.getStatusIcon(status);
    const uptimeClass = this.getUptimeClass(monitor.uptime_percent);

    div.innerHTML = `
      <div class="monitor-header">
        <div class="monitor-name">${this.escapeHtml(monitor.service || 'Unknown')}</div>
        <div class="monitor-status status-${status}">${statusIcon} ${monitor.status || 'PENDING'}</div>
      </div>
      <div class="monitor-details">
        <div class="monitor-detail">
          <span class="detail-label">Host:</span>
          <span class="detail-value">${this.escapeHtml(monitor.host || 'N/A')}</span>
        </div>
        <div class="monitor-detail">
          <span class="detail-label">Response Time:</span>
          <span class="detail-value">${this.escapeHtml(monitor.response_time || 'N/A')}</span>
        </div>
        <div class="monitor-detail">
          <span class="detail-label">Uptime:</span>
          <span class="detail-value ${uptimeClass}">${monitor.uptime_percent !== undefined ? monitor.uptime_percent.toFixed(2) + '%' : 'N/A'}</span>
        </div>
        <div class="monitor-detail">
          <span class="detail-label">Last Check:</span>
          <span class="detail-value">${this.escapeHtml(monitor.last_check || 'Never')}</span>
        </div>
        ${monitor.details ? `
        <div class="monitor-detail">
          <span class="detail-label">Details:</span>
          <span class="detail-value">${this.escapeHtml(monitor.details)}</span>
        </div>
        ` : ''}
      </div>
    `;

    return div;
  }

  getStatusIcon(status) {
    const icons = {
      'ok': '✅',
      'warning': '⚠️',
      'failure': '❌',
      'pending': '⏳'
    };
    return icons[status] || '⏳';
  }

  getUptimeClass(uptime) {
    if (uptime === undefined || uptime === null) return '';
    if (uptime >= 99) return 'good';
    if (uptime >= 95) return 'warning';
    return 'bad';
  }

  escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  startAutoRefresh() {
    this.stopAutoRefresh();
    this.refreshTimer = setInterval(() => {
      if (this.isOnline && !document.hidden) {
        this.loadStatus();
      }
    }, this.refreshInterval);
  }

  stopAutoRefresh() {
    if (this.refreshTimer) {
      clearInterval(this.refreshTimer);
      this.refreshTimer = null;
    }
  }

  updateConnectionStatus() {
    const statusDot = document.getElementById('connection-status');
    const statusText = document.getElementById('connection-text');

    if (this.isOnline) {
      statusDot.className = 'status-dot status-ok';
      statusText.textContent = 'Connected';
    } else {
      statusDot.className = 'status-dot status-error';
      statusText.textContent = 'Offline';
    }
  }

  // Notification methods
  async requestNotificationPermission() {
    if (!('Notification' in window)) {
      alert('This browser does not support notifications');
      return;
    }

    if (!this.swRegistration) {
      alert('Service Worker not registered');
      return;
    }

    try {
      const permission = await Notification.requestPermission();

      if (permission === 'granted') {
        console.log('[Argus] Notification permission granted');
        await this.subscribeToPush();
        this.updateNotificationButton();
        this.hideNotificationModal();
      } else {
        console.log('[Argus] Notification permission denied');
        alert('Notification permission denied. You can enable it in browser settings.');
      }
    } catch (error) {
      console.error('[Argus] Error requesting notification permission:', error);
      alert('Failed to request notification permission');
    }
  }

  async subscribeToPush() {
    try {

      // Check if already subscribed
      let subscription = await this.swRegistration.pushManager.getSubscription();

      if (!subscription) {
        const keyBytes = this.urlBase64ToUint8Array('BNaf3sfdXZqUeRqEmKb10CI4XbxZcq1ApvrqHSoyX7cXvKP_rH6P4PkRD28IyvwMQhnuxW-Qa-3jrGC5PCl97TM');
        console.log("Length:", keyBytes.byteLength, "First byte:", keyBytes[0]);
        subscription = await this.swRegistration.pushManager.subscribe({
          userVisibleOnly: true,
          applicationServerKey: keyBytes
        });

        console.log('[Argus] Push subscription created:', subscription);
      }

      // Send subscription to server
      const response = await fetch(this.apiBaseUrl + this.baseUrl + this.subscribeEndpoint, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(subscription)
      });

      if (response.ok) {
        console.log('[Argus] Subscription sent to server');
        localStorage.setItem('argus-push-subscribed', 'true');
      } else {
        console.error('[Argus] Failed to send subscription to server:', response.status);
      }

    } catch (error) {
      console.error('[Argus] Error subscribing to push:', error);
    }
  }

  async unsubscribeFromPush() {
    try {
      const subscription = await this.swRegistration.pushManager.getSubscription();

      if (subscription) {
        // Unsubscribe
        await subscription.unsubscribe();
        console.log('[Argus] Unsubscribed from push');

        // Notify server
        await fetch(this.apiBaseUrl + this.baseUrl + this.unsubscribeEndpoint, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(subscription)
        });

        localStorage.removeItem('argus-push-subscribed');
        this.updateNotificationButton();
      }
    } catch (error) {
      console.error('[Argus] Error unsubscribing from push:', error);
    }
  }

  updateNotificationButton() {
    const btn = document.getElementById('notification-btn');

    if (!('Notification' in window)) {
      btn.disabled = true;
      btn.title = 'Notifications not supported';
      return;
    }

    const permission = Notification.permission;

    if (permission === 'granted') {
      btn.classList.add('enabled');
      btn.classList.remove('disabled');
      btn.title = 'Notifications enabled';
      btn.querySelector('.btn-text').textContent = 'Notify ✓';
    } else if (permission === 'denied') {
      btn.classList.add('disabled');
      btn.classList.remove('enabled');
      btn.title = 'Notifications blocked - check browser settings';
    } else {
      btn.classList.remove('enabled', 'disabled');
      btn.title = 'Enable push notifications';
    }
  }

  showNotificationModal() {
    if (Notification.permission === 'granted') {
      alert('Notifications are already enabled! ✅');
      return;
    }

    if (Notification.permission === 'denied') {
      alert('Notifications are blocked. Please enable them in your browser settings.');
      return;
    }

    document.getElementById('notification-modal').style.display = 'flex';
  }

  hideNotificationModal() {
    document.getElementById('notification-modal').style.display = 'none';
  }

  // Install methods
  checkInstallPrompt() {
    // Show install prompt if app is not installed
    if (window.matchMedia('(display-mode: standalone)').matches) {
      console.log('[Argus] App is running in standalone mode');
      document.getElementById('install-prompt').style.display = 'none';
    } else if (this.deferredPrompt) {
      document.getElementById('install-prompt').style.display = 'block';
    }
  }

  showInstallModal() {
    document.getElementById('install-modal').style.display = 'flex';
  }

  hideInstallModal() {
    document.getElementById('install-modal').style.display = 'none';
  }

  async installApp() {
    if (!this.deferredPrompt) {
      // Show iOS/Android instructions
      alert('To install:\n\niOS: Tap Share button, then "Add to Home Screen"\n\nAndroid: Tap menu button, then "Install app"');
      return;
    }

    this.deferredPrompt.prompt();
    const { outcome } = await this.deferredPrompt.userChoice;

    console.log('[Argus] Install prompt outcome:', outcome);

    if (outcome === 'accepted') {
      console.log('[Argus] User accepted the install prompt');
    } else {
      console.log('[Argus] User dismissed the install prompt');
    }

    this.deferredPrompt = null;
    this.hideInstallModal();
  }

  // Helper methods
  urlBase64ToUint8Array(base64String) {
    const padding = '='.repeat((4 - base64String.length % 4) % 4);
    const base64 = (base64String + padding)
      .replace(/\-/g, '+')
      .replace(/_/g, '/');

    const rawData = window.atob(base64);
    const outputArray = new Uint8Array(rawData.length);

    for (let i = 0; i < rawData.length; ++i) {
      outputArray[i] = rawData.charCodeAt(i);
    }
    return outputArray;
  }
}

// Initialize app when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    window.argusApp = new ArgusMonitor();
  });
} else {
  window.argusApp = new ArgusMonitor();
}
