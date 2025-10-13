/**
 * Main application initialization for Argus Monitor
 * Orchestrates all components and manages application lifecycle
 */

class ArgusApp {
  constructor() {
    this.config = null;
    this.ui = null;
    this.notifications = null;
    this.refreshInterval = null;
    this.updateIntervalMs = 30000; // 30 seconds
  }

  /**
   * Initialize the application
   */
  async init() {
    try {
      // Initialize configuration
      this.config = new ArgusConfig();
      await this.config.loadConfig();

      // Initialize UI
      this.ui = new ArgusUI(this.config);

      // Initialize notifications
      this.notifications = new ArgusNotifications(this.config);
      await this.notifications.registerServiceWorker();

      // Setup service worker update checking
      this.setupServiceWorkerUpdates();

      // Setup event listeners
      this.setupEventListeners();

      // Initial data load
      await this.ui.updateMonitorData();

      // Start auto-refresh
      this.startAutoRefresh();

      console.log('Argus Monitor initialized successfully');
    } catch (error) {
      console.error('Failed to initialize Argus Monitor:', error);
      this.ui?.showError('Failed to initialize application');
    }
  }

  /**
   * Setup service worker update detection and auto-reload
   */
  async setupServiceWorkerUpdates() {
    if (!('serviceWorker' in navigator)) {
      return;
    }

    try {
      const registration = await navigator.serviceWorker.getRegistration();
      if (!registration) {
        return;
      }

      // Listen for service worker updates
      registration.addEventListener('updatefound', () => {
        const newWorker = registration.installing;
        console.log('[App] Service worker update found, installing...');

        newWorker.addEventListener('statechange', () => {
          if (newWorker.state === 'installed' && navigator.serviceWorker.controller) {
            // New version available - prompt user to reload
            console.log('[App] New version available, prompting user to reload');
            this.promptForUpdate(newWorker);
          }
        });
      });

      // Check for updates periodically (every minute)
      setInterval(() => {
        console.log('[App] Checking for service worker updates...');
        registration.update();
      }, 60000);

      // Check for updates immediately
      registration.update();

      console.log('[App] Service worker update checking enabled');
    } catch (error) {
      console.error('[App] Failed to setup service worker updates:', error);
    }
  }

  /**
   * Prompt user to reload the application for updates
   */
  promptForUpdate(newWorker) {
    // Show update notification in UI
    const updateBanner = document.createElement('div');
    updateBanner.id = 'update-banner';
    updateBanner.style.cssText = `
      position: fixed;
      top: 0;
      left: 0;
      right: 0;
      background: #2196F3;
      color: white;
      padding: 12px 20px;
      text-align: center;
      z-index: 10000;
      box-shadow: 0 2px 5px rgba(0,0,0,0.2);
    `;
    updateBanner.innerHTML = `
      <span style="margin-right: 15px;">A new version is available!</span>
      <button id="update-reload-btn" style="
        background: white;
        color: #2196F3;
        border: none;
        padding: 6px 16px;
        border-radius: 4px;
        cursor: pointer;
        font-weight: bold;
        margin-right: 10px;
      ">Reload Now</button>
      <button id="update-dismiss-btn" style="
        background: transparent;
        color: white;
        border: 1px solid white;
        padding: 6px 16px;
        border-radius: 4px;
        cursor: pointer;
      ">Later</button>
    `;

    document.body.insertBefore(updateBanner, document.body.firstChild);

    // Handle reload button
    document.getElementById('update-reload-btn').addEventListener('click', () => {
      // Tell service worker to skip waiting and activate immediately
      newWorker.postMessage({ type: 'SKIP_WAITING' });
      // Reload the page
      window.location.reload();
    });

    // Handle dismiss button
    document.getElementById('update-dismiss-btn').addEventListener('click', () => {
      updateBanner.remove();
    });
  }

  /**
   * Setup event listeners for UI interactions
   */
  setupEventListeners() {
    // Refresh button
    const refreshBtn = document.getElementById('refresh-btn');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', () => this.handleRefresh());
    }

    // Notification button
    const notifyBtn = document.getElementById('notification-btn');
    if (notifyBtn) {
      notifyBtn.addEventListener('click', () => this.handleNotificationToggle());
    }

    // Handle visibility change to pause/resume updates
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) {
        this.stopAutoRefresh();
      } else {
        this.startAutoRefresh();
        this.ui.updateMonitorData(); // Refresh immediately when page becomes visible
      }
    });
  }

  /**
   * Handle refresh button click
   */
  async handleRefresh() {
    const btn = document.getElementById('refresh-btn');
    btn.disabled = true;
    try {
      await this.ui.updateMonitorData();
    } finally {
      btn.disabled = false;
    }
  }

  /**
   * Handle notification toggle button click
   */
  async handleNotificationToggle() {
    await this.notifications.togglePushSubscription();
  }

  /**
   * Start auto-refresh interval
   */
  startAutoRefresh() {
    if (this.refreshInterval) {
      return; // Already running
    }
    this.refreshInterval = setInterval(() => {
      this.ui.updateMonitorData();
    }, this.updateIntervalMs);
  }

  /**
   * Stop auto-refresh interval
   */
  stopAutoRefresh() {
    if (this.refreshInterval) {
      clearInterval(this.refreshInterval);
      this.refreshInterval = null;
    }
  }

  /**
   * Set custom update interval
   */
  setUpdateInterval(ms) {
    this.updateIntervalMs = ms;
    if (this.refreshInterval) {
      this.stopAutoRefresh();
      this.startAutoRefresh();
    }
  }
}

// Initialize application when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    window.argusApp = new ArgusApp();
    window.argusApp.init();
  });
} else {
  window.argusApp = new ArgusApp();
  window.argusApp.init();
}
