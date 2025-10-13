/**
 * UI management for Argus Monitor
 * Handles rendering monitor data and updating the interface
 */

class ArgusUI {
  constructor(config) {
    this.config = config;
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
      this.renderMonitorData(data);
      this.updateTimestamp(data.timestamp);
    } catch (error) {
      console.error('Error:', error);
      this.showError('Error loading data');
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
    html += '<th>Service</th><th>Host</th><th>Status</th>';
    html += '<th>Time</th><th>Up%</th><th>Check</th><th>Details</th>';
    html += '</tr></thead>';
    html += '</table></div>';

    document.getElementById('content').innerHTML = html;
  }

  /**
   * Render a single monitor row
   */
  renderMonitorRow(monitor) {
    const statusClass = this.getStatusClass(monitor.status);
    const uptimePercent = monitor.uptime_percent.toFixed(0);
    const lastCheck = monitor.last_check.split(' ')[1] || 'Never';
    const details = monitor.details.substring(0, 20);

    let html = '<tr>';
    html += '<td>' + this.escapeHtml(monitor.service) + '</td>';
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
