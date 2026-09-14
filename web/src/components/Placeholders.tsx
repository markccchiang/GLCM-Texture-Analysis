// Panels whose content arrives in later phases of doc/ui-design-plan.md.

import { Text } from '@mantine/core';
import { useQuery } from '@tanstack/react-query';
import type { ReactNode } from 'react';
import { getCatalog } from '../api/client';

function PanelSection({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="panel-section" aria-label={title}>
      <header className="panel-header">{title}</header>
      <div className="panel-body">{children}</div>
    </section>
  );
}

export function RoiManagerPlaceholder() {
  return (
    <PanelSection title="ROI Manager">
      <Text size="sm" c="dimmed">
        Rectangle, ellipse, polygon and freehand ROIs arrive in phase 3.
      </Text>
    </PanelSection>
  );
}

export function AnalysisSettingsPlaceholder() {
  const catalog = useQuery({ queryKey: ['catalog'], queryFn: ({ signal }) => getCatalog(signal) });
  return (
    <PanelSection title="Analysis Settings">
      <Text size="sm" c="dimmed">
        Feature selection and GLCM parameters arrive in phase 4.
      </Text>
      {catalog.data && (
        <Text size="xs" c="dimmed" mt="xs">
          The server offers {catalog.data.features.length} features in {catalog.data.presets.length} presets (Ng {catalog.data.limits.minGrayLevels}–
          {catalog.data.limits.maxGrayLevels}, default {catalog.data.limits.defaultGrayLevels}).
        </Text>
      )}
      {catalog.isError && (
        <Text size="xs" c="red" mt="xs">
          The server could not be reached: {catalog.error.message}
        </Text>
      )}
    </PanelSection>
  );
}

export function ResultsPlaceholder() {
  return (
    <PanelSection title="Results">
      <Text size="sm" c="dimmed">
        Measurement results arrive in phase 4.
      </Text>
    </PanelSection>
  );
}
