/**
 * Configuration management for Argus Monitor
 * Handles base URL detection and configuration loading
 */

class ArgusConfig {
  constructor() {
    this.baseUrl = '';
  }

  /**
   * Detect and load base URL from path or server config
   */
  async loadConfig() {
    try {
      // Try to detect from current URL path first
      const path = window.location.pathname;
      if (path && path !== '/' && !path.endsWith('.html')) {
        // Extract base path (e.g., /argus from /argus/ or /argus/index.html)
        const pathParts = path.split('/').filter(p => p);
        if (pathParts.length > 0) {
          this.baseUrl = '/' + pathParts[0];
        }
      }

      // Then try to fetch config to confirm
      const configUrl = this.baseUrl ? this.baseUrl + '/config.json' : '/config.json';
      const response = await fetch(configUrl);
      if (response.ok) {
        const config = await response.json();
        if (config.base_url) {
          this.baseUrl = config.base_url;
        }
      }
    } catch (e) {
      console.log('Could not load config from server:', e);
      // Keep the detected baseUrl from path
    }
    console.log('Using base_url:', this.baseUrl);
  }

  /**
   * Get the configured base URL
   */
  getBaseUrl() {
    return this.baseUrl;
  }

  /**
   * Build a full URL with base URL prefix
   */
  buildUrl(path) {
    return this.baseUrl + path;
  }
}
