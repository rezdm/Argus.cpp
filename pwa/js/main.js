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
