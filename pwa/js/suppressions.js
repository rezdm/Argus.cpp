/**
 * Notification suppression management
 * Handles suppressing notifications for selected tests
 */

class SuppressionsManager {
  constructor(config, ui) {
    this.config = config;
    this.ui = ui;
  }

  /**
   * Show suppression dialog
   */
  showSuppressionDialog() {
    const selectedTests = this.ui.getSelectedTests();
    if (selectedTests.length === 0) {
      alert('Please select at least one test to suppress notifications.');
      return;
    }

    // Calculate default time (now + 24 hours)
    const now = new Date();
    now.setHours(now.getHours() + 24);
    const defaultDateTime = this.formatDateTime(now);

    // Create dialog
    const dialog = document.createElement('div');
    dialog.className = 'modal';
    dialog.innerHTML = `
      <div class="modal-content">
        <h2>Suppress Notifications</h2>
        <p>Suppress notifications for <strong>${selectedTests.length}</strong> selected test(s) until:</p>
        <div class="form-group">
          <label for="suppress-until">Date and Time:</label>
          <input type="datetime-local" id="suppress-until" value="${defaultDateTime}" required>
        </div>
        <div class="modal-actions">
          <button class="btn btn-primary" id="suppress-confirm">Suppress</button>
          <button class="btn btn-secondary" id="suppress-cancel">Cancel</button>
        </div>
      </div>
    `;

    document.body.appendChild(dialog);

    // Setup event listeners
    document.getElementById('suppress-confirm').addEventListener('click', () => {
      const untilInput = document.getElementById('suppress-until');
      const until = untilInput.value;
      if (until) {
        this.suppressTests(selectedTests, this.convertToServerFormat(until));
      }
      document.body.removeChild(dialog);
    });

    document.getElementById('suppress-cancel').addEventListener('click', () => {
      document.body.removeChild(dialog);
    });

    // Close on background click
    dialog.addEventListener('click', (e) => {
      if (e.target === dialog) {
        document.body.removeChild(dialog);
      }
    });
  }

  /**
   * Suppress notifications for tests
   */
  async suppressTests(testIds, until) {
    try {
      const response = await fetch(this.config.buildUrl('/push/suppress'), {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          test_ids: testIds,
          until: until
        })
      });

      if (!response.ok) {
        throw new Error('Failed to suppress notifications');
      }

      const result = await response.json();
      alert(`Successfully suppressed notifications for ${result.suppressed_count} test(s) until ${until}`);

      // Clear selections
      this.ui.clearSelections();

      // Refresh UI to show suppressed status
      await this.ui.updateMonitorData();
    } catch (error) {
      console.error('Error suppressing tests:', error);
      alert('Failed to suppress notifications. Please try again.');
    }
  }

  /**
   * Unsuppress notifications for tests
   */
  async unsuppressTests(testIds) {
    try {
      const response = await fetch(this.config.buildUrl('/push/unsuppress'), {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          test_ids: testIds
        })
      });

      if (!response.ok) {
        throw new Error('Failed to unsuppress notifications');
      }

      const result = await response.json();
      alert(`Successfully restored notifications for ${result.unsuppressed_count} test(s)`);

      // Refresh UI to show updated suppressed status
      await this.ui.updateMonitorData();
    } catch (error) {
      console.error('Error unsuppressing tests:', error);
      alert('Failed to restore notifications. Please try again.');
    }
  }

  /**
   * List all suppressions
   */
  async listSuppressions() {
    try {
      const response = await fetch(this.config.buildUrl('/push/suppressions'));
      if (!response.ok) {
        throw new Error('Failed to fetch suppressions');
      }

      const result = await response.json();
      return result.suppressions;
    } catch (error) {
      console.error('Error listing suppressions:', error);
      return {};
    }
  }

  /**
   * Show suppressions dialog
   */
  async showSuppressionsDialog() {
    const suppressions = await this.listSuppressions();
    const suppressionCount = Object.keys(suppressions).length;

    if (suppressionCount === 0) {
      alert('No active notification suppressions.');
      return;
    }

    // Create dialog
    const dialog = document.createElement('div');
    dialog.className = 'modal';

    let suppressionsList = '';
    for (const [testId, until] of Object.entries(suppressions)) {
      suppressionsList += `
        <div class="suppression-item">
          <span class="test-id">${this.escapeHtml(testId)}</span>
          <span class="until">until ${this.escapeHtml(until)}</span>
          <button class="btn btn-small unsuppress-btn" data-test-id="${this.escapeHtml(testId)}">Remove</button>
        </div>
      `;
    }

    dialog.innerHTML = `
      <div class="modal-content">
        <h2>Active Notification Suppressions</h2>
        <p>${suppressionCount} test(s) have suppressed notifications:</p>
        <div class="suppressions-list">
          ${suppressionsList}
        </div>
        <div class="modal-actions">
          <button class="btn btn-secondary" id="suppressions-close">Close</button>
        </div>
      </div>
    `;

    document.body.appendChild(dialog);

    // Setup event listeners
    document.getElementById('suppressions-close').addEventListener('click', () => {
      document.body.removeChild(dialog);
    });

    // Setup unsuppress buttons
    dialog.querySelectorAll('.unsuppress-btn').forEach(btn => {
      btn.addEventListener('click', async (e) => {
        const testId = e.target.getAttribute('data-test-id');
        await this.unsuppressTests([testId]);
        document.body.removeChild(dialog);
        // Reopen dialog to show updated list
        this.showSuppressionsDialog();
      });
    });

    // Close on background click
    dialog.addEventListener('click', (e) => {
      if (e.target === dialog) {
        document.body.removeChild(dialog);
      }
    });
  }

  /**
   * Format date for datetime-local input
   */
  formatDateTime(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    return `${year}-${month}-${day}T${hours}:${minutes}`;
  }

  /**
   * Convert datetime-local format to server format
   */
  convertToServerFormat(datetimeLocal) {
    // Convert from "2025-10-15T09:00" to "2025-10-15 09:00:00"
    const date = new Date(datetimeLocal);
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = '00';
    return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
  }

  /**
   * Escape HTML to prevent XSS
   */
  escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }
}
