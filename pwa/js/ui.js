/**
 * UI management for Argus Monitor
 * Handles rendering monitor data and updating the interface
 */

class ArgusUI {
  constructor(config) {
    this.config = config;
    this.selectedTests = new Set(); // Track selected test IDs
    this.suppressedTests = new Set(); // Track suppressed test IDs
  }

  /**
   * Get CSS class for monitor status
   */
  getStatusClass(status) {
    switch (status.toLowerCase()) {
      case 'pending': return 'status-pending';
      case 'ok': return 'status-ok';
      case 'warning': return 'status-warning';
      case 'failure': return 'status-error';
      default: return 'status-pending';
    }
  }

  /**
   * Escape HTML to prevent XSS
   */
  escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  /**
   * Fetch and update monitor data from server
   */
  async updateMonitorData() {
    try {
      const response = await fetch(this.config.buildUrl('/status'));
      if (!response.ok) throw new Error('Network error');

      const data = await response.json();

      // Also fetch suppression status
      await this.updateSuppressionStatus();

      this.renderMonitorData(data);
      this.updateTimestamp(data.timestamp);
    } catch (error) {
      console.error('Error:', error);
      this.showError('Error loading data');
    }
  }

  /**
   * Fetch current suppression status from server
   */
  async updateSuppressionStatus() {
    try {
      const response = await fetch(this.config.buildUrl('/push/suppressions'));
      if (response.ok) {
        const data = await response.json();
        this.suppressedTests.clear();
        for (const testId of Object.keys(data.suppressions)) {
          this.suppressedTests.add(testId);
        }
        this.updateSuppressionButtonBadge();
      }
    } catch (error) {
      console.debug('Failed to fetch suppression status:', error);
      // Don't fail the entire update if suppressions can't be fetched
    }
  }

  /**
   * Update the suppression button badge with count
   */
  updateSuppressionButtonBadge() {
    const suppressionsBtn = document.getElementById('suppressions-btn');
    if (suppressionsBtn) {
      const count = this.suppressedTests.size;
      if (count > 0) {
        suppressionsBtn.textContent = `🔕 (${count})`;
        suppressionsBtn.classList.add('has-suppressions');
      } else {
        suppressionsBtn.textContent = '🔕';
        suppressionsBtn.classList.remove('has-suppressions');
      }
    }
  }

  /**
   * Render monitor data to the page
   */
  renderMonitorData(data) {
    // Update page title
    document.getElementById('page-title').textContent = data.name;
    document.title = data.name + ' - Monitor';

    let html = '';

    // Render each monitor group
    data.groups.forEach((group) => {
      html += '<div class="group">';
      html += '<div class="group-header">' + this.escapeHtml(group.name) + '</div>';
      html += '<table class="monitor-table"><tbody>';

      group.monitors.forEach(monitor => {
        html += this.renderMonitorRow(monitor);
      });

      html += '</tbody></table></div>';
    });

    // Add unified header at the bottom
    html += '<div class="group footer-header">';
    html += '<table class="monitor-table">';
    html += '<thead><tr>';
    html += '<th></th><th>Service</th><th>Host</th><th>Status</th>';
    html += '<th>Time</th><th>Up%</th><th>Check</th><th>Details</th>';
    html += '</tr></thead>';
    html += '</table></div>';

    document.getElementById('content').innerHTML = html;

    // Setup checkbox listeners after DOM update
    this.setupCheckboxListeners();
    this.updateIgnoreButtonState();
  }

  /**
   * Render a single monitor row
   */
  renderMonitorRow(monitor) {
    const statusClass = this.getStatusClass(monitor.status);
    const uptimePercent = monitor.uptime_percent.toFixed(0);
    const lastCheck = monitor.last_check.split(' ')[1] || 'Never';
    const details = monitor.details || 'N/A';
    const testId = monitor.id || '';
    const isChecked = this.selectedTests.has(testId) ? ' checked' : '';
    const isSuppressed = this.suppressedTests.has(testId);
    const suppressedClass = isSuppressed ? ' suppressed-row' : '';
    const suppressedIndicator = isSuppressed ? ' <span class="suppressed-badge" title="Notifications suppressed">🔕</span>' : '';

    let html = '<tr class="' + suppressedClass + '">';
    html += '<td><input type="checkbox" class="test-checkbox" data-test-id="' + this.escapeHtml(testId) + '"' + isChecked + '></td>';
    html += '<td>' + this.escapeHtml(monitor.service) + suppressedIndicator + '</td>';
    html += '<td>' + this.escapeHtml(monitor.host) + '</td>';
    html += '<td class="' + statusClass + '">' + this.escapeHtml(monitor.status) + '</td>';
    html += '<td>' + this.escapeHtml(monitor.response_time) + '</td>';
    html += '<td>';
    html += '<div class="uptime-bar">';
    html += '<div class="uptime-fill" style="width: ' + monitor.uptime_percent + '%"></div>';
    html += '</div>';
    html += uptimePercent + '%';
    html += '</td>';
    html += '<td>' + this.escapeHtml(lastCheck) + '</td>';
    html += '<td>' + this.escapeHtml(details) + '</td>';
    html += '</tr>';

    return html;
  }

  /**
   * Setup checkbox event listeners after rendering
   */
  setupCheckboxListeners() {
    const checkboxes = document.querySelectorAll('.test-checkbox');
    checkboxes.forEach(checkbox => {
      checkbox.addEventListener('change', (e) => {
        const testId = e.target.getAttribute('data-test-id');
        if (e.target.checked) {
          this.selectedTests.add(testId);
        } else {
          this.selectedTests.delete(testId);
        }
        this.updateIgnoreButtonState();
      });
    });
  }

  /**
   * Update ignore button state based on selections
   */
  updateIgnoreButtonState() {
    const ignoreBtn = document.getElementById('ignore-btn');
    if (ignoreBtn) {
      ignoreBtn.disabled = this.selectedTests.size === 0;
      ignoreBtn.textContent = this.selectedTests.size > 0 ?
        `Ignore (${this.selectedTests.size})` : 'Ignore';
    }
  }

  /**
   * Get selected test IDs
   */
  getSelectedTests() {
    return Array.from(this.selectedTests);
  }

  /**
   * Clear test selections
   */
  clearSelections() {
    this.selectedTests.clear();
    const checkboxes = document.querySelectorAll('.test-checkbox');
    checkboxes.forEach(checkbox => checkbox.checked = false);
    this.updateIgnoreButtonState();
  }

  /**
   * Update the last updated timestamp
   */
  updateTimestamp(timestamp) {
    const timeOnly = timestamp.split(' ')[1];
    document.getElementById('last-updated-bar').textContent = 'Updated: ' + timeOnly;
  }

  /**
   * Show error message
   */
  showError(message) {
    document.getElementById('content').innerHTML =
      '<div class="error">' + this.escapeHtml(message) + '</div>';
  }
}
